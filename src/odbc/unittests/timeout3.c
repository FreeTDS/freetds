/*
 * Test connection timeout
 */

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

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#include <freetds/tds.h>
#include <freetds/replacements.h>
#include <freetds/utils.h>

#include "fake_thread.h"

#if TDS_HAVE_MUTEX

static void init_connect(void);

static void
init_connect(void)
{
	CHKAllocEnv(&odbc_env, "S");
	SQLSetEnvAttr(odbc_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
	CHKAllocConnect(&odbc_conn, "S");
}

static tds_mutex mtx;
static TDS_SYS_SOCKET fake_sock;

/* accept a socket and read data as much as you can */
TDS_THREAD_PROC_DECLARE(fake_thread_proc, arg)
{
	TDS_SYS_SOCKET s = TDS_PTR2INT(arg), sock;
	socklen_t len;
	char buf[128];
	struct sockaddr_in sin;
	struct pollfd fd;

	memset(&sin, 0, sizeof(sin));
	len = sizeof(sin);

	fd.fd = s;
	fd.events = POLLIN;
	fd.revents = 0;
	if (poll(&fd, 1, 30000) <= 0) {
		fprintf(stderr, "poll: %d\n", sock_errno);
		exit(1);
	}

	if (TDS_IS_SOCKET_INVALID(sock = tds_accept(s, (struct sockaddr *) &sin, &len))) {
		perror("accept");
		exit(1);
	}
	tds_socket_set_nodelay(sock);
	tds_mutex_lock(&mtx);
	fake_sock = sock;
	tds_mutex_unlock(&mtx);
	CLOSESOCKET(s);

	for (;;) {
		int len;

		fd.fd = sock;
		fd.events = POLLIN;
		fd.revents = 0;
		if (poll(&fd, 1, 30000) <= 0) {
			fprintf(stderr, "poll: %d\n", sock_errno);
			exit(1);
		}

		/* just read and discard */
		len = READSOCKET(sock, buf, sizeof(buf));
		if (len == 0)
			break;
		if (len < 0 && sock_errno != TDSSOCK_EINPROGRESS)
			break;
	}
	return TDS_THREAD_RESULT(0);
}

TEST_MAIN()
{
	SQLTCHAR tmp[2048];
	char conn[128];
	SQLTCHAR sqlstate[6];
	SQLSMALLINT len;
	int port;
	time_t start_time, end_time;

	tds_socket_init();

	if (tds_mutex_init(&mtx))
		return 1;

	if (odbc_read_login_info())
		exit(1);

	/*
	 * prepare our odbcinst.ini 
	 * it is better to do it before connecting because unixODBC caches INIs
	 * the name must be odbcinst.ini because unixODBC accepts only this name
	 */
	if (common_pwd.driver[0]) {
		FILE *f = fopen("odbcinst.ini", "w");

		if (f) {
			fprintf(f, "[FreeTDS]\nDriver = %s\n", common_pwd.driver);
			fclose(f);
			/* force iODBC */
			setenv("ODBCINSTINI", "./odbcinst.ini", 1);
			setenv("SYSODBCINSTINI", "./odbcinst.ini", 1);
			/* force unixODBC (only directory) */
			setenv("ODBCSYSINI", ".", 1);
		}
	}

	/* this test requires version 7.0, avoid to override externally */
	setenv("TDSVER", "7.0", 1);
	unsetenv("TDSPORT");

	for (port = 12340; port < 12350; ++port)
		if (init_fake_server(port))
			break;
	if (port == 12350) {
		fprintf(stderr, "Cannot bind to a port\n");
		return 1;
	}
	printf("Fake server bound at port %d\n", port);

	init_connect();
	CHKSetConnectAttr(SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER) 25, sizeof(SQLINTEGER), "SI");
	CHKSetConnectAttr(SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER) 10, SQL_IS_UINTEGER, "SI");

	/* this is expected to work with unixODBC */
	printf("try to connect to our port just to check connection timeout\n");
	sprintf(conn, "DRIVER=FreeTDS;SERVER=127.0.0.1;Port=%d;TDS_Version=7.0;UID=test;PWD=test;DATABASE=tempdb;", port);
	start_time = time(NULL);
	CHKDriverConnect(NULL, T(conn), SQL_NTS, tmp, TDS_VECTOR_SIZE(tmp), &len, SQL_DRIVER_NOPROMPT, "E");
	end_time = time(NULL);

	memset(sqlstate, 'X', sizeof(sqlstate));
	tmp[0] = 0;
	CHKGetDiagRec(SQL_HANDLE_DBC, odbc_conn, 1, sqlstate, NULL, tmp, TDS_VECTOR_SIZE(tmp), NULL, "SI");
	odbc_disconnect();
	tds_mutex_lock(&mtx);
	CLOSESOCKET(fake_sock);
	tds_mutex_unlock(&mtx);
	tds_thread_join(fake_thread, NULL);

	printf("Message: %s - %s\n", C(sqlstate), C(tmp));
	if (strcmp(C(sqlstate), "HYT00") || !strstr(C(tmp), "Timeout")) {
		fprintf(stderr, "Invalid timeout message\n");
		return 1;
	}
	if (end_time - start_time < 10 || end_time - start_time > 16) {
		fprintf(stderr, "Unexpected connect timeout (%d)\n", (int) (end_time - start_time));
		return 1;
	}

	printf("Done.\n");
	ODBC_FREE();
	return 0;
}

#else	/* !TDS_HAVE_MUTEX */
TEST_MAIN()
{
	printf("Not possible for this platform.\n");
	odbc_test_skipped();
	return 0;
}
#endif
