/* Check some internal token behaviour */

#include "common.h"

#include <assert.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <freetds/time.h>

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#include <freetds/tds.h>
#include <freetds/replacements.h>
#include <freetds/server.h>
#include <freetds/utils.h>

#include "fake_thread.h"
#include "parser.h"

#if TDS_HAVE_MUTEX

#ifdef _WIN32
#define SHUT_RDWR SD_BOTH
#endif

static void parse_sql(TDSSOCKET * tds, const char *sql);

static void
setup_override(void)
{
	char buf[128];
	FILE *f;

	sprintf(buf, "tokens_pwd.%d", (int) getpid());
	f = fopen(buf, "w");
	assert(f);
	fprintf(f, "UID=guest\nPWD=sybase\nSRV=%s\nDB=tempdb\n", odbc_server);
	fclose(f);
	rename(buf, "tokens_pwd");
	unlink(buf);
	setenv("TDSPWDFILE", "tokens_pwd", 1);
	unsetenv("TDSINIOVERRIDE");

	unsetenv("TDSHOST");
	unsetenv("TDSPORT");
	unsetenv("TDSVER");
}

static TDSSOCKET *
tds_from_sock(TDSCONTEXT *ctx, TDS_SYS_SOCKET fd)
{
	TDSSOCKET *tds;

	tds = tds_alloc_socket(ctx, 4096);
	if (!tds) {
		CLOSESOCKET(fd);
		fprintf(stderr, "out of memory");
		return NULL;
	}
	tds_set_s(tds, fd);
	tds->out_flag = TDS_LOGIN;
	/* TODO proper charset */
	tds_iconv_open(tds->conn, "ISO8859-1", 0);
	/* get_incoming(tds->s); */
	tds->state = TDS_IDLE;

	tds->conn->client_spid = 0x33;
	tds->conn->product_version = TDS_MS_VER(10, 0, 6000);

	return tds;
}

static void handle_one(TDS_SYS_SOCKET sock);
static TDS_SYS_SOCKET stop_socket = INVALID_SOCKET;

/* accept a socket and emulate a server */
TDS_THREAD_PROC_DECLARE(fake_thread_proc, arg)
{
	TDS_SYS_SOCKET s = TDS_PTR2INT(arg), sock;
	socklen_t len;
	struct sockaddr_in sin;
	struct pollfd fds[2];
	TDS_SYS_SOCKET sockets[2];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) >= 0);
	stop_socket = sockets[1];

	for (;;) {
		fds[0].fd = s;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		fds[1].fd = sockets[0];
		fds[1].events = POLLIN;
		fds[1].revents = 0;
		if (poll(fds, 2, 30000) <= 0) {
			fprintf(stderr, "poll: %d\n", sock_errno);
			exit(1);
		}
		if (fds[1].revents)
			break;

		memset(&sin, 0, sizeof(sin));
		len = sizeof(sin);
		if (TDS_IS_SOCKET_INVALID(sock = tds_accept(s, (struct sockaddr *) &sin, &len))) {
			perror("accept");
			exit(1);
		}
		tds_socket_set_nodelay(sock);
		handle_one(sock);
	}
	CLOSESOCKET(s);
	CLOSESOCKET(sockets[0]);
	CLOSESOCKET(sockets[1]);

	return TDS_THREAD_RESULT(0);
}

static void
handle_one(TDS_SYS_SOCKET sock)
{
	TDSSOCKET *tds;
	TDSLOGIN *login;
	TDSCONTEXT *ctx;

	ctx = tds_alloc_context(NULL);
	if (!ctx)
		exit(1);

	tds = tds_from_sock(ctx, sock);
	if (!tds)
		exit(1);

	login = tds_alloc_read_login(tds);
	if (!login) {
		fprintf(stderr, "Error reading login\n");
		exit(1);
	}

	if (!strcmp(tds_dstr_cstr(&login->user_name), "guest") && !strcmp(tds_dstr_cstr(&login->password), "sybase")) {
		tds->out_flag = TDS_REPLY;
		tds_env_change(tds, TDS_ENV_DATABASE, "master", "pubs2");
		if (IS_TDS71_PLUS(tds->conn))
			tds_put_n(tds, "\xe3\x08\x00\x07\x05\x09\x04\xd0\x00\x34\x00", 11);
		tds_send_msg(tds, 5701, 2, 10, "Changed database context to 'pubs2'.", "JDBC", NULL, 1);
		if (!login->suppress_language) {
			tds_env_change(tds, TDS_ENV_LANG, NULL, "us_english");
			tds_send_msg(tds, 5703, 1, 10, "Changed language setting to 'us_english'.", "JDBC", NULL, 1);
		}
		if (IS_TDS50(tds->conn))
			tds_env_change(tds, TDS_ENV_PACKSIZE, NULL, "512");
		tds_send_login_ack(tds, "Microsoft SQL Server");
		if (!IS_TDS50(tds->conn))
			tds_env_change(tds, TDS_ENV_PACKSIZE, "4096", "4096");
		if (IS_TDS50(tds->conn))
			tds_send_capabilities_token(tds);
		tds_send_done_token(tds, TDS_DONE_FINAL, 0);
	} else {
		exit(1);
	}
	tds_flush_packet(tds);
	tds_free_login(login);
	login = NULL;

	for (;;) {
		const char *query;

		query = tds_get_generic_query(tds);
		if (!query)
			break;
		printf("query : %s\n", query);

		tds->out_flag = TDS_REPLY;

		if (strncmp(query, "use tempdb", 10) == 0) {
			tds_env_change(tds, TDS_ENV_DATABASE, "pubs2", "tempdb");
			if (IS_TDS71_PLUS(tds->conn))
				tds_put_n(tds, "\xe3\x08\x00\x07\x05\x09\x04\xd0\x00\x34\x00", 11);
			tds_send_msg(tds, 5701, 2, 10, "Changed database context to 'tempdb'.", "JDBC", NULL, 1);
			tds_send_done_token(tds, TDS_DONE_FINAL, 0);
			tds_flush_packet(tds);
			continue;
		}

		if (strncmp(query, "SET TEXTSIZE ", 13) == 0) {
			tds_send_done_token(tds, TDS_DONE_FINAL, 0);
			tds_flush_packet(tds);
			continue;
		}

		parse_sql(tds, query);
		continue;
	}
	shutdown(sock, SHUT_RDWR);
	tds_close_socket(tds);
	tds_free_socket(tds);
	tds_free_context(ctx);
	tds_free_query();
}

static char *
read_line(void *param, char *s, size_t size)
{
	const char **const p = (const char **) param;
	const char *start = *p, *end;
	size_t len;

	if (!*start)
		return NULL;
	end = strchr(start, '\n');
	end = end ? end + 1 : strchr(start, 0);
	len = end - start;
	if (len >= size) {
		fprintf(stderr, "Line too long\n");
		exit(1);
	}
	memcpy(s, start, len);
	s[len] = 0;
	*p = start + len;
	return s;
}

static void
parse_sql(TDSSOCKET *tds, const char *sql)
{
	bool cond = true;
	odbc_parser *parser = odbc_init_parser_func(read_line, &sql);
	int done_token;
	TDSRESULTINFO *resinfo;
	TDSCOLUMN *col;

	resinfo = tds_alloc_results(1);
	if (!resinfo) {
		fprintf(stderr, "out of memory");
		exit(1);
	}
	col = resinfo->columns[0];
	tds_set_column_type(tds->conn, col, SYBVARCHAR);
	col->column_size = 30;
	col->on_server.column_size = 30;
	if (!tds_dstr_copy(&col->column_name, "name"))
		exit(1);
	col->column_cur_size = 8;
	col->column_data = (void *) "row data";

	for (;;) {
		char *p;
		const char *cmd = odbc_get_cmd_line(parser, &p, &cond);

		if (!cmd)
			break;

		if (!strcmp(cmd, "info")) {
			if (!cond)
				continue;

			/* RAISERROR */
			tds_send_msg(tds, 50000, 1, 5, "information message", "JDBC", NULL, 1);
			continue;
		}

		if (!strcmp(cmd, "error")) {
			if (!cond)
				continue;

			/* division by zero */
			tds_send_err(tds, 8134, 1, 16, "division by zero", "JDBC", NULL, 1);
			continue;
		}

		if (!strcmp(cmd, "rowfmt")) {
			if (!cond)
				continue;

			tds_send_table_header(tds, resinfo);
			continue;
		}

		if (!strcmp(cmd, "row")) {
			if (!cond)
				continue;

			tds_send_row(tds, resinfo);
			continue;
		}

		if (!strcmp(cmd, "quit")) {
			if (!cond)
				continue;
			shutdown(tds->conn->s, SHUT_RDWR);
			break;
		}

		if (!strcmp(cmd, "done"))
			done_token = TDS_DONE_TOKEN;
		else if (!strcmp(cmd, "doneinproc"))
			done_token = TDS_DONEINPROC_TOKEN;
		else
			done_token = 0;
		if (done_token) {
			const char *tok;
			TDS_SMALLINT flags = TDS_DONE_MORE_RESULTS;
			int rows = -1;
			bool nocount = false;

			while ((tok = odbc_get_tok(&p)) != NULL) {
				if (!strcmp(tok, "final"))
					flags &= ~TDS_DONE_MORE_RESULTS;
				else if (!strcmp(tok, "error"))
					flags |= TDS_DONE_ERROR;
				else if (!strcmp(tok, "nocount"))
					nocount = true;
				else
					rows = atoi(tok);
			}

			if (!cond)
				continue;

			if (rows >= 0)
				flags |= nocount ? 0 : TDS_DONE_COUNT;
			else
				rows = 0;
			tds_send_done(tds, done_token, flags, rows);
			continue;
		}
		odbc_fatal(parser, ": unknown command '%s'\n", cmd);
	}
	tds_free_results(resinfo);
	odbc_free_parser(parser);
	tds_flush_packet(tds);
}

/*
 * Fetch tests:
 * All reply starts with ROWFMT + ROW, we test behaviour or fetch:
 * - is there a row?
 * - is there an error or info?
 * - is there row count and how much
 */
static void
test_fetch(const char *replies, const char *expected_no_row, const char *expected_row, int line)
{
	char *sql;
	ODBC_BUF *odbc_buf = NULL;
	int expected_rows = -1;
	char state[12] = "No";
	SQLLEN char_data_ind;
	char char_data[64];

	printf("Testing SQLFetch, line %d\n", line);

	CHKBindCol(1, SQL_C_CHAR, char_data, sizeof(char_data), &char_data_ind, "S");

	sql = odbc_buf_asprintf(&odbc_buf, "rowfmt\nrow\n%s", replies);

	CHKExecDirect(T(sql), SQL_NTS, "S");
	CHKFetch("S");
	sscanf(expected_no_row, " %10s %d", state, &expected_rows);
	CHKFetch(state);
	ODBC_CHECK_ROWS(expected_rows);
	while (CHKMoreResults("SENo") != SQL_NO_DATA)
		continue;

	sql = odbc_buf_asprintf(&odbc_buf, "rowfmt\nrow\nrow\n%s", replies);

	CHKExecDirect(T(sql), SQL_NTS, "S");
	ODBC_CHECK_ROWS(-1);
	CHKFetch("S");
	strcpy(state, "S");
	expected_rows = -1;
	sscanf(expected_row, " %10s %d", state, &expected_rows);
	CHKFetch(state);
	ODBC_CHECK_ROWS(expected_rows);
	while (CHKMoreResults("SIENo") != SQL_NO_DATA)
		continue;

	odbc_reset_statement();
	ODBC_FREE();
}

#define test_fetch(r, e1, e2) test_fetch(r, e1, e2, __LINE__)

int
main(void)
{
	int port;
	char connect[100];

	tds_socket_init();

	for (port = 12340; port < 12350; ++port)
		if (init_fake_server(port))
			break;
	if (port == 12350) {
		fprintf(stderr, "Cannot bind to a port\n");
		return 1;
	}
	printf("Fake server bound at port %d\n", port);

	odbc_read_login_info();
	setup_override();

	odbc_use_version3 = 1;
	sprintf(connect, "SERVER=127.0.0.1,%d;TDS_Version=7.3;UID=guest;PWD=sybase;DATABASE=tempdb;Encrypt=No;", port);
	odbc_conn_additional_params = connect;
	odbc_connect();

	/*
	 * Test cases for SQLFetch (with and without row)
	 */

	/* info + done with row */
	test_fetch("info\ndone 1\ndone final 2", "No 1", "");

	/* info + done with row */
	if (!odbc_driver_is_freetds())
		test_fetch("info\ndone error 1\ndone final 2", "No 1", "");

	/* info + done with row */
	if (!odbc_driver_is_freetds())
		test_fetch("info\ndone error\ndone final 2", "No 0", "");

	/* error + done with row */
	test_fetch("error\ndone 1\ndone final", "E 1", "");

	/* error + done with row */
	test_fetch("error\ndone error 1\ndone final", "E 1", "");

	/* error + done with row */
	if (!odbc_driver_is_freetds())
		test_fetch("error\ndone error\ndone final 3", "E 0", "");

	/* info + doneinproc with row */
	test_fetch("info\ndoneinproc 1\ndone final", "No 1", "");

	/* info + doneinproc with row */
	if (!odbc_driver_is_freetds())
		test_fetch("info\ndoneinproc error 1\ndone final", "No 1", "");

	/* info + doneinproc with row */
	if (!odbc_driver_is_freetds())
		test_fetch("info\ndoneinproc error\ndone final 3", "No 0", "");

	/* error + doneinproc with row */
	if (!odbc_driver_is_freetds())
		test_fetch("error\ndoneinproc 1\ndoneinproc 2\ndone final", "E 1", "");

	/* error + doneinproc with row */
	if (!odbc_driver_is_freetds())
		test_fetch("error\ndoneinproc error 1\ndoneinproc 2\ndone final", "E 1", "");

	/* error + doneinproc with row */
	if (!odbc_driver_is_freetds())
		test_fetch("error\ndoneinproc error\ndoneinproc 2\ndone final", "E 0", "");

	/* doneinproc with row + doneinproc with different row */
	if (!odbc_driver_is_freetds())
		test_fetch("doneinproc 1\ndoneinproc 2\ndone final", "No 1", "S 1");

	/* doneinproc with row + done with different row */
	if (!odbc_driver_is_freetds())
		test_fetch("doneinproc 1\ndone 2\ndone final", "No 1", "S 1");

	/* done with row + done with different row */
	if (!odbc_driver_is_freetds())
		test_fetch("done 1\ndone 2\ndone final", "No 1", "S 1");

	/* doneinproc without row + doneinproc with rows */
	if (!odbc_driver_is_freetds())
		test_fetch("doneinproc\ndoneinproc 2\ndone final 3", "No 0", "S 0");

	/* doneinproc with row but not count flag + doneinproc with rows */
	if (!odbc_driver_is_freetds())
		test_fetch("doneinproc nocount 5\ndoneinproc 2\ndone final 3", "No 5", "S 5");

	odbc_disconnect();

	odbc_use_version3 = 0;
	odbc_connect();

	/*
	 * Test cases for SQLFetch (with and without row)
	 */

	/* info + done with row */
	test_fetch("info\ndone 1\ndone final 2", "No 1", "");

	/* info + done with row */
	if (!odbc_driver_is_freetds())
		test_fetch("info\ndone error 1\ndone final 2", "No 1", "");

	/* info + done with row */
	if (!odbc_driver_is_freetds())
		test_fetch("info\ndone error\ndone final 2", "No 0", "");

	/* error + done with row */
	test_fetch("error\ndone 1\ndone final", "E 1", "");

	/* error + done with row */
	test_fetch("error\ndone error 1\ndone final", "E 1", "");

	/* error + done with row */
	if (!odbc_driver_is_freetds())
		test_fetch("error\ndone error\ndone final 3", "E 0", "");

	/* info + doneinproc with row */
	test_fetch("info\ndoneinproc 1\ndone final", "No 1", "");

	/* info + doneinproc with row */
	if (!odbc_driver_is_freetds())
		test_fetch("info\ndoneinproc error 1\ndone final", "No 1", "");

	/* info + doneinproc with row */
	if (!odbc_driver_is_freetds())
		test_fetch("info\ndoneinproc error\ndone final 3", "No 0", "");

	/* error + doneinproc with row */
	test_fetch("error\ndoneinproc 1\ndoneinproc 2\ndone final", "E 2", "");

	/* error + doneinproc with row */
	test_fetch("error\ndoneinproc error 1\ndoneinproc 2\ndone final", "E 2", "");

	/* error + doneinproc with row */
	test_fetch("error\ndoneinproc error\ndoneinproc 2\ndone final", "E 2", "");

	/* doneinproc with row + doneinproc with different row */
	if (!odbc_driver_is_freetds())
		test_fetch("doneinproc 1\ndoneinproc 2\ndone final", "No 2", "S 2");

	/* doneinproc with row + done with different row */
	if (!odbc_driver_is_freetds())
		test_fetch("doneinproc 1\ndone 2\ndone final", "No 1", "S 1");

	/* done with row + done with different row */
	if (!odbc_driver_is_freetds())
		test_fetch("done 1\ndone 2\ndone final", "No 1", "S 1");

	/* doneinproc without row + doneinproc with rows */
	if (!odbc_driver_is_freetds())
		test_fetch("doneinproc\ndoneinproc 2\ndone final 3", "No 2", "S 2");

	odbc_disconnect();

	shutdown(stop_socket, SHUT_RDWR);
	tds_thread_join(fake_thread, NULL);

	return 0;
}

#else /* !TDS_HAVE_MUTEX */
int
main(void)
{
	printf("Not possible for this platform.\n");
	odbc_test_skipped();
	return 0;
}
#endif
