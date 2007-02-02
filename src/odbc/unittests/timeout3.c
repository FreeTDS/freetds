#include "common.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#include "tds.h"

/*
	test connection timeout
*/

static char software_version[] = "$Id: timeout3.c,v 1.2.2.2 2007-02-02 16:23:31 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/* TODO port to windows, use thread */
#if HAVE_FORK && HAVE_ALARM

static void init_connect(void);

static void
init_connect(void)
{
	if (SQLAllocEnv(&Environment) != SQL_SUCCESS) {
		printf("Unable to allocate env\n");
		exit(1);
	}

	SQLSetEnvAttr(Environment, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);

	if (SQLAllocConnect(Environment, &Connection) != SQL_SUCCESS) {
		printf("Unable to allocate connection\n");
		SQLFreeEnv(Environment);
		exit(1);
	}
}

static int
init_fake_server(int ip_port)
{
	struct sockaddr_in sin;
	TDS_SYS_SOCKET fd, s;
	socklen_t len;
	pid_t pid;
	char buf[128];

	memset(&sin, 0, sizeof(sin));
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((short) ip_port);
	sin.sin_family = AF_INET;

	if (TDS_IS_SOCKET_INVALID(s = socket(AF_INET, SOCK_STREAM, 0))) {
		perror("socket");
		exit(1);
	}
	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		perror("bind");
		CLOSESOCKET(s);
		return 1;
	}
	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}
	if (pid > 0) {
		CLOSESOCKET(s);
		return 0;
	}

	listen(s, 5);
	len = sizeof(sin);
	if ((fd = accept(s, (struct sockaddr *) &sin, &len)) < 0) {
		perror("accept");
		exit(1);
	}
	CLOSESOCKET(s);

	alarm(30);
	for (;;) {
		/* just read and discard */
		len = READSOCKET(fd, buf, sizeof(buf));
		if (len == 0)
			break;
		if (len < 0 && sock_errno != TDSSOCK_EINPROGRESS)
			break;
	}
	exit(0);
}

int
main(int argc, char *argv[])
{
	int res;
	char tmp[2048];
	char sqlstate[6];
	SQLSMALLINT len;
	int port;
	time_t start_time, end_time;

	if (read_login_info())
		exit(1);

	/*
	 * prepare our odbcinst.ini 
	 * is better to do it before connect cause uniODBC cache INIs
	 * the name must be odbcinst.ini cause unixODBC accept only this name
	 */
	if (DRIVER[0]) {
		FILE *f = fopen("odbcinst.ini", "w");

		if (f) {
			fprintf(f, "[FreeTDS]\nDriver = %s\n", DRIVER);
			fclose(f);
			/* force iODBC */
			setenv("ODBCINSTINI", "./odbcinst.ini", 1);
			setenv("SYSODBCINSTINI", "./odbcinst.ini", 1);
			/* force unixODBC (only directory) */
			setenv("ODBCSYSINI", ".", 1);
		}
	}

	init_connect();
	for (port = 12340; port < 12350; ++port)
		if (!init_fake_server(port))
			break;
	if (port == 12350) {
		fprintf(stderr, "Cannot bind to a port\n");
		return 1;
	}
	printf("Fake server binded at port %d\n", port);

	res = SQLSetConnectAttr(Connection, SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER) 10, sizeof(SQLINTEGER));
	if (!SQL_SUCCEEDED(res))
		ODBC_REPORT_ERROR("SQLSetConnectAttr error");
	res = SQLSetConnectAttr(Connection, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER) 10, sizeof(SQLINTEGER));
	if (!SQL_SUCCEEDED(res))
		ODBC_REPORT_ERROR("SQLSetConnectAttr error");

	/* this is expected to work with unixODBC */
	printf("try to connect to our port just to check connection timeout\n");
	sprintf(tmp, "DRIVER=FreeTDS;SERVER=127.0.0.1;Port=%d;TDS_Version=7.0;UID=test;PWD=test;DATABASE=tempdb;", port);
	start_time = time(NULL);
	res = SQLDriverConnect(Connection, NULL, (SQLCHAR *) tmp, SQL_NTS, (SQLCHAR *) tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT);
	if (SQL_SUCCEEDED(res)) {
		fprintf(stderr, "SQLDriverConnect should fail (res=%d)\n", (int) res);
		return 1;
	}
	end_time = time(NULL);

	strcpy(sqlstate, "XXXXX");
	tmp[0] = 0;
	res = SQLGetDiagRec(SQL_HANDLE_DBC, Connection, 1, (SQLCHAR *) sqlstate, NULL, (SQLCHAR *) tmp, sizeof(tmp), NULL);
	if (!SQL_SUCCEEDED(res)) {
		printf("SQLGetDiagRec should not fail\n");
		return 1;
	}
	Disconnect();

	printf("Message: %s - %s\n", sqlstate, tmp);
	if (strcmp(sqlstate, "HYT00") || !strstr(tmp, "Timeout")) {
		fprintf(stderr, "Invalid timeout message\n");
		return 1;
	}
	if (end_time - start_time < 10 || end_time - start_time > 16) {
		fprintf(stderr, "Unexpected connect timeout (%d)\n", (int) (end_time - start_time));
		return 1;
	}

	printf("Done.\n");
	return 0;
}

#else
int
main(void)
{
	printf("Not possible for this platform.\n");
	return 0;
}
#endif
