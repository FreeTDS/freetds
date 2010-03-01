#include "common.h"

/* TODO port to windows, use thread */
#if defined(TDS_HAVE_PTHREAD_MUTEX) && HAVE_ALARM

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

#include <pthread.h>

#include "tds.h"

/*
	test connection timeout
*/

static char software_version[] = "$Id: timeout3.c,v 1.11 2010-03-01 14:50:55 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void init_connect(void);

static void
init_connect(void)
{
	CHKAllocEnv(&Environment, "S");
	SQLSetEnvAttr(Environment, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) (SQL_OV_ODBC3), SQL_IS_UINTEGER);
	CHKAllocConnect(&Connection, "S");
}

static pthread_t      fake_thread;
static TDS_SYS_SOCKET fake_sock;

static void *fake_thread_proc(void * arg);

static int
init_fake_server(int ip_port)
{
	struct sockaddr_in sin;
	TDS_SYS_SOCKET s;
	int err;

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
	listen(s, 5);
	err = pthread_create(&fake_thread, NULL, fake_thread_proc, int2ptr(s));
	if (err != 0) {
		perror("pthread_create");
		exit(1);
	}
	return 0;
}

static void *
fake_thread_proc(void * arg)
{
	TDS_SYS_SOCKET s = ptr2int(arg);
	socklen_t len;
	char buf[128];
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	len = sizeof(sin);
	alarm(30);
	if (TDS_IS_SOCKET_INVALID(fake_sock = tds_accept(s, (struct sockaddr *) &sin, &len))) {
		perror("accept");
		exit(1);
	}
	CLOSESOCKET(s);

	for (;;) {
		/* just read and discard */
		len = READSOCKET(fake_sock, buf, sizeof(buf));
		if (len == 0)
			break;
		if (len < 0 && sock_errno != TDSSOCK_EINPROGRESS)
			break;
	}
	return NULL;
}

int
main(int argc, char *argv[])
{
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

	for (port = 12340; port < 12350; ++port)
		if (!init_fake_server(port))
			break;
	if (port == 12350) {
		fprintf(stderr, "Cannot bind to a port\n");
		return 1;
	}
	printf("Fake server binded at port %d\n", port);

	init_connect();
	CHKSetConnectAttr(SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER) 10, sizeof(SQLINTEGER), "SI");
	CHKSetConnectAttr(SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER) 10, sizeof(SQLINTEGER), "SI");

	/* this is expected to work with unixODBC */
	printf("try to connect to our port just to check connection timeout\n");
	sprintf(tmp, "DRIVER=FreeTDS;SERVER=127.0.0.1;Port=%d;TDS_Version=7.0;UID=test;PWD=test;DATABASE=tempdb;", port);
	start_time = time(NULL);
	CHKDriverConnect(NULL, (SQLCHAR *) tmp, SQL_NTS, (SQLCHAR *) tmp, sizeof(tmp), &len, SQL_DRIVER_NOPROMPT, "E");
	end_time = time(NULL);

	strcpy(sqlstate, "XXXXX");
	tmp[0] = 0;
	CHKGetDiagRec(SQL_HANDLE_DBC, Connection, 1, (SQLCHAR *) sqlstate, NULL, (SQLCHAR *) tmp, sizeof(tmp), NULL, "SI");
	Disconnect();
	CLOSESOCKET(fake_sock);
	pthread_join(fake_thread, NULL);

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

#else	/* !TDS_HAVE_PTHREAD_MUTEX */
int
main(void)
{
	printf("Not possible for this platform.\n");
	return 0;
}
#endif
