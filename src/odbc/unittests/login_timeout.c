/*
 * Test login timeout if server accepts connection but does not dialog.
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

#ifdef _WIN32
#define SHUT_RDWR SD_BOTH
#endif

#include "fake_thread.h"

static void init_connect(void);

static void
init_connect(void)
{
	CHKAllocEnv(&odbc_env, "S");
	SQLSetEnvAttr(odbc_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
	CHKAllocConnect(&odbc_conn, "S");
}

static TDS_SYS_SOCKET listen_sock;

/* accept a socket and read data as much as you can */
TDS_THREAD_PROC_DECLARE(fake_thread_proc, arg)
{
	TDS_SYS_SOCKET s = TDS_PTR2INT(arg);
	int accepted = 0;

	listen_sock = s;

	for (;;) {
		socklen_t len;
		struct sockaddr_in sin;
		struct pollfd fd;
		TDS_SYS_SOCKET sock;

		fd.fd = s;
		fd.events = POLLIN;
		fd.revents = 0;
		if (poll(&fd, 1, 30000) <= 0)
			break;

		memset(&sin, 0, sizeof(sin));
		len = sizeof(sin);
		if (TDS_IS_SOCKET_INVALID(sock = tds_accept(s, (struct sockaddr *) &sin, &len)))
			break;
		++accepted;
	}
	CLOSESOCKET(s);
	return TDS_THREAD_RESULT(accepted);
}

TEST_MAIN()
{
	SQLTCHAR tmp[2048];
	char conn[128];
	SQLTCHAR sqlstate[6];
	SQLSMALLINT len;
	int port, accepted;
	void *res;
	time_t start_time, end_time;

	tds_socket_init();

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

	/* this test requires version "auto", avoid to override externally */
	setenv("TDSVER", "auto", 1);
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
	CHKSetConnectAttr(SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER) 8, sizeof(SQLINTEGER), "SI");
	CHKSetConnectAttr(SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER) 2, SQL_IS_UINTEGER, "SI");

	printf("try to connect to our port just to check connection timeout\n");
	sprintf(conn, "DRIVER=FreeTDS;SERVER=127.0.0.1;Port=%d;TDS_Version=auto;UID=test;PWD=test;DATABASE=tempdb;", port);
	start_time = time(NULL);
	CHKDriverConnect(NULL, T(conn), SQL_NTS, tmp, TDS_VECTOR_SIZE(tmp), &len, SQL_DRIVER_NOPROMPT, "E");
	end_time = time(NULL);

	memset(sqlstate, 'X', sizeof(sqlstate));
	tmp[0] = 0;
	CHKGetDiagRec(SQL_HANDLE_DBC, odbc_conn, 1, sqlstate, NULL, tmp, TDS_VECTOR_SIZE(tmp), NULL, "SI");
	odbc_disconnect();
	shutdown(listen_sock, SHUT_RDWR);
	tds_thread_join(fake_thread, &res);
	accepted = TDS_PTR2INT(res);

	printf("Message: %s - %s\n", C(sqlstate), C(tmp));
	if (strcmp(C(sqlstate), "HYT00") || !strstr(C(tmp), "Timeout")) {
		fprintf(stderr, "Invalid timeout message\n");
		return 1;
	}
	if (end_time - start_time != 9) {
		fprintf(stderr, "Unexpected connect timeout (%d)\n", (int) (end_time - start_time));
		return 1;
	}
	if (accepted < 2 || accepted > 4) {
		fprintf(stderr, "Unexpected connection attempts (%d)\n", accepted);
		return 1;
	}

	printf("Done.\n");
	ODBC_FREE();
	return 0;
}
