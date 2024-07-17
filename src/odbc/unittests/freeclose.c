#include "common.h"

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

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif /* HAVE_SYS_WAIT_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#if (!defined(TDS_NO_THREADSAFE) && HAVE_ALARM && HAVE_FSTAT && defined(S_IFSOCK)) || defined(_WIN32)

#include <ctype.h>

#include <freetds/tds.h>
#include <freetds/utils.h>

#include "fake_thread.h"

/* this crazy test tests that we do not send too much prepare ... */

static tds_mutex mtx;

typedef union {
	struct sockaddr sa;
	struct sockaddr_in sin;
	char dummy[256];
} long_sockaddr;

static long_sockaddr remote_addr;
static socklen_t remote_addr_len;

#ifdef _WIN32
#define alarm(n) do { ; } while(0)
#endif

static void
write_all(TDS_SYS_SOCKET s, const void *buf, int len)
{
	int res, l;
	fd_set fds_write;

	for (; len > 0;) {
		FD_ZERO(&fds_write);
		FD_SET(s, &fds_write);

		res = select(s + 1, NULL, &fds_write, NULL, NULL);
		if (res <= 0) {
			if (errno == EINTR)
				continue;
			perror("select");
			exit(1);
		}

		l = WRITESOCKET(s, buf, len);
		if (l <= 0) {
			perror("write socket");
			exit(1);
		}
		buf = ((const char *) buf) + l;
		len -= l;
	}
}

static unsigned int inserts = 0;
static char insert_buf[256] = "";

static void
count_insert(const char* buf, size_t len)
{
	static const char search[] = "insert into";
	static unsigned char prev = 'x';
	char *p;
	unsigned char c;
	size_t insert_len;

	/* add to buffer */
	for (p = strchr(insert_buf, 0); len > 0; --len, ++buf) {
		c = (unsigned char) buf[0];
		if (prev == 0 || c != 0)
			*p++ = (c >= ' ' && c < 128) ? tolower(c) : '_';
		prev = c;
	}
	*p = 0;

	/* check it */
	while ((p=strstr(insert_buf, search)) != NULL) {
		tds_mutex_lock(&mtx);
		++inserts;
		tds_mutex_unlock(&mtx);
		/* do not find again */
		p[0] = '_';
	}

	/* avoid buffer too long */
	insert_len = strlen(insert_buf);
	if (insert_len > sizeof(search)) {
		p = insert_buf + insert_len - sizeof(search);
		memmove(insert_buf, p, strlen(p) + 1);
	}
}

static unsigned int round_trips = 0;
static enum { sending, receiving } flow = sending;

TDS_THREAD_PROC_DECLARE(fake_thread_proc, arg)
{
	TDS_SYS_SOCKET s = TDS_PTR2INT(arg), server_sock;
	socklen_t sock_len;
	int len;
	char buf[128];
	struct sockaddr_in sin;
	fd_set fds_read, fds_write, fds_error;
	TDS_SYS_SOCKET max_fd = 0;
	TDS_SYS_SOCKET fake_sock;

	memset(&sin, 0, sizeof(sin));
	sock_len = sizeof(sin);
	alarm(30);
	fprintf(stderr, "waiting connect...\n");
	if ((fake_sock = tds_accept(s, (struct sockaddr *) &sin, &sock_len)) < 0) {
		perror("accept");
		exit(1);
	}
	tds_socket_set_nodelay(fake_sock);
	CLOSESOCKET(s);

	if (TDS_IS_SOCKET_INVALID(server_sock = socket(remote_addr.sa.sa_family, SOCK_STREAM, 0))) {
		perror("socket");
		exit(1);
	}

	fprintf(stderr, "connecting to server...\n");
	if (remote_addr.sa.sa_family == AF_INET) {
		fprintf(stderr, "connecting to %x:%d\n", (unsigned int) remote_addr.sin.sin_addr.s_addr,
			ntohs(remote_addr.sin.sin_port));
	}
	if (connect(server_sock, &remote_addr.sa, remote_addr_len)) {
		perror("connect");
		exit(1);
	}
	alarm(0);

	if (fake_sock > max_fd) max_fd = fake_sock;
	if (server_sock > max_fd) max_fd = server_sock;

	for (;;) {
		int res;

		FD_ZERO(&fds_read);
		FD_SET(fake_sock, &fds_read);
		FD_SET(server_sock, &fds_read);

		FD_ZERO(&fds_write);

		FD_ZERO(&fds_error);
		FD_SET(fake_sock, &fds_error);
		FD_SET(server_sock, &fds_error);

		alarm(30);
		res = select(max_fd + 1, &fds_read, &fds_write, &fds_error, NULL);
		alarm(0);
		if (res < 0) {
			if (sock_errno == TDSSOCK_EINTR)
				continue;
			perror("select");
			exit(1);
		}

		if (FD_ISSET(fake_sock, &fds_error) || FD_ISSET(server_sock, &fds_error)) {
			fprintf(stderr, "error in select\n");
			exit(1);
		}

		/* just read and forward */
		if (FD_ISSET(fake_sock, &fds_read)) {
			if (flow != sending) {
				tds_mutex_lock(&mtx);
				++round_trips;
				tds_mutex_unlock(&mtx);
			}
			flow = sending;

			len = READSOCKET(fake_sock, buf, sizeof(buf));
			if (len == 0) {
				fprintf(stderr, "client connection closed\n");
				break;
			}
			if (len < 0 && sock_errno != TDSSOCK_EINPROGRESS) {
				fprintf(stderr, "read client error %d\n", sock_errno);
				break;
			}
			count_insert(buf, len);
			write_all(server_sock, buf, len);
		}

		if (FD_ISSET(server_sock, &fds_read)) {
			if (flow != receiving) {
				tds_mutex_lock(&mtx);
				++round_trips;
				tds_mutex_unlock(&mtx);
			}
			flow = receiving;

			len = READSOCKET(server_sock, buf, sizeof(buf));
			if (len == 0) {
				fprintf(stderr, "server connection closed\n");
				break;
			}
			if (len < 0 && sock_errno != TDSSOCK_EINPROGRESS) {
				fprintf(stderr, "read server error %d\n", sock_errno);
				break;
			}
			write_all(fake_sock, buf, len);
		}
	}
	CLOSESOCKET(fake_sock);
	CLOSESOCKET(server_sock);
	return TDS_THREAD_RESULT(0);
}

TEST_MAIN()
{
	SQLLEN sql_nts = SQL_NTS;
	const char *query;
	SQLINTEGER id = 0;
	char string[64];
	TDS_SYS_SOCKET last_socket;
	int port;
	const int num_inserts = 20;
	int is_freetds;

	tds_socket_init();

	if (tds_mutex_init(&mtx))
		return 1;

	odbc_mark_sockets_opened();

	odbc_connect();

	/*
	 * this does not work if server is not connected with socket
	 * (ie ms driver connected locally)
	 */
	last_socket = odbc_find_last_socket();
	if (TDS_IS_SOCKET_INVALID(last_socket)) {
		fprintf(stderr, "Error finding last socket opened\n");
		return 1;
	}

	remote_addr_len = sizeof(remote_addr);
	if (tds_getpeername(last_socket, &remote_addr.sa, &remote_addr_len)) {
		fprintf(stderr, "Unable to get remote address %d\n", sock_errno);
		return 1;
	}

	is_freetds = odbc_driver_is_freetds();
	odbc_disconnect();

	/* init fake server, behave like a proxy */
	for (port = 12340; port < 12350; ++port)
		if (init_fake_server(port))
			break;
	if (port == 12350) {
		fprintf(stderr, "Cannot bind to a port\n");
		return 1;
	}
	printf("Fake server bound at port %d\n", port);

	/* override connections */
	if (is_freetds) {
		setenv("TDSHOST", "127.0.0.1", 1);
		sprintf(string, "%d", port);
		setenv("TDSPORT", string, 1);

		odbc_connect();
	} else {
		char tmp[2048];
		SQLSMALLINT len;

		CHKAllocEnv(&odbc_env, "S");
		CHKAllocConnect(&odbc_conn, "S");
		sprintf(tmp,
			"DRIVER={SQL Server};SERVER=127.0.0.1,%d;UID=%s;"
			"PWD=%s;DATABASE=%s;Network=DBMSSOCN;", port, common_pwd.user, common_pwd.password, common_pwd.database);
		printf("connection string: %s\n", tmp);
		CHKDriverConnect(NULL, T(tmp), SQL_NTS, (SQLTCHAR *) tmp, sizeof(tmp) / sizeof(SQLTCHAR), &len,
				 SQL_DRIVER_NOPROMPT, "SI");
		CHKAllocStmt(&odbc_stmt, "S");
	}

	/* real test */
	odbc_command("CREATE TABLE #test(i int, c varchar(40))");

	odbc_reset_statement();

	/* do not take into account connection statistics */
	tds_mutex_lock(&mtx);
	round_trips = 0;
	inserts = 0;
	tds_mutex_unlock(&mtx);

	query = "insert into #test values (?, ?)";

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, sizeof(id), 0, &id, 0, &sql_nts, "SI");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(string), 0, string, 0, &sql_nts, "SI");

	CHKPrepare(T(query), SQL_NTS, "SI");
	tds_mutex_lock(&mtx);
	printf("%u round trips %u inserts\n", round_trips, inserts);
	tds_mutex_unlock(&mtx);

	for (id = 0; id < num_inserts; id++) {
		sprintf(string, "This is a test (%d)", (int) id);
		CHKExecute("SI");
		CHKFreeStmt(SQL_CLOSE, "S");
	}

	tds_mutex_lock(&mtx);
	printf("%u round trips %u inserts\n", round_trips, inserts);
	tds_mutex_unlock(&mtx);
	odbc_reset_statement();

	tds_mutex_lock(&mtx);
	if (inserts > 1 || round_trips > (unsigned) (num_inserts * 2 + 6)) {
		fprintf(stderr, "Too much round trips (%u) or insert (%u) !!!\n", round_trips, inserts);
		tds_mutex_unlock(&mtx);
		return 1;
	}
	printf("%u round trips %u inserts\n", round_trips, inserts);
	tds_mutex_unlock(&mtx);

#ifdef ENABLE_DEVELOPING
	/* check for SQL_RESET_PARAMS */
	tds_mutex_lock(&mtx);
	round_trips = 0;
	inserts = 0;
	tds_mutex_unlock(&mtx);

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, sizeof(id), 0, &id, 0, &sql_nts, "SI");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(string), 0, string, 0, &sql_nts, "SI");

	CHKPrepare((SQLCHAR *) query, SQL_NTS, "SI");
	tds_mutex_lock(&mtx);
	printf("%u round trips %u inserts\n", round_trips, inserts);
	tds_mutex_unlock(&mtx);

	for (id = 0; id < num_inserts; id++) {
		CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, sizeof(id), 0, &id, 0, &sql_nts, "SI");
		CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, sizeof(string), 0, string, 0, &sql_nts, "SI");

		sprintf(string, "This is a test (%d)", (int) id);
		CHKExecute("SI");
		CHKFreeStmt(SQL_RESET_PARAMS, "S");
	}

	tds_mutex_lock(&mtx);
	printf("%u round trips %u inserts\n", round_trips, inserts);
	tds_mutex_unlock(&mtx);
	odbc_reset_statement();

	tds_mutex_lock(&mtx);
	if (inserts > 1 || round_trips > num_inserts * 2 + 6) {
		fprintf(stderr, "Too much round trips (%u) or insert (%u) !!!\n", round_trips, inserts);
		tds_mutex_unlock(&mtx);
		return 1;
	}
	printf("%u round trips %u inserts\n", round_trips, inserts);
	tds_mutex_unlock(&mtx);
#endif

	odbc_disconnect();

	alarm(10);
	tds_thread_join(fake_thread, NULL);

	return 0;
}

#else
TEST_MAIN()
{
	printf("Not possible for this platform.\n");
	odbc_test_skipped();
	return 0;
}
#endif

