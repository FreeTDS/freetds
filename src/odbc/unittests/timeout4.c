#include "common.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <freetds/time.h>

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

/*
 * test error on connection close 
 * With a trick we simulate a connection close then we try to 
 * prepare or execute a query. This should fail and return an error message.
 */

#if HAVE_FSTAT && defined(S_IFSOCK)

static int end_socket = -1;

static int
shutdown_last_socket(void)
{
	TDS_SYS_SOCKET max_socket = odbc_find_last_socket();
	TDS_SYS_SOCKET sockets[2];

	if (max_socket < 0)
		return 0;

	/* replace socket with a new one */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
		return 0;

	/* substitute socket */
	close(max_socket);
	dup2(sockets[0], max_socket);

	/* close connection */
	close(sockets[0]);
	end_socket = sockets[1];
	return 1;
}

static int
Test(int direct)
{
	SQLTCHAR buf[256];
	SQLTCHAR sqlstate[6];
	time_t start_time, end_time;

	odbc_mark_sockets_opened();
	odbc_connect();

	if (!shutdown_last_socket()) {
		fprintf(stderr, "Error shutting down connection\n");
		return 1;
	}

	CHKSetStmtAttr(SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER) 10, SQL_IS_UINTEGER, "S");

	alarm(30);
	start_time = time(NULL);
	if (direct) {
		CHKExecDirect(T("SELECT 1"), SQL_NTS, "E");
	} else {
		SQLSMALLINT cols;
		/* force dialog with server */
		if (CHKPrepare(T("SELECT 1"), SQL_NTS, "SE") == SQL_SUCCESS)
			CHKNumResultCols(&cols, "E");
	}
	end_time = time(NULL);
	alarm(0);

	memset(sqlstate, 'X', sizeof(sqlstate));
	CHKGetDiagRec(SQL_HANDLE_STMT, odbc_stmt, 1, sqlstate, NULL, buf, ODBC_VECTOR_SIZE(buf), NULL, "SI");
	sqlstate[5] = 0;
	printf("Message: %s - %s\n", C(sqlstate), C(buf));
	if (strcmp(C(sqlstate), "HYT00") || !strstr(C(buf), "Timeout")) {
		fprintf(stderr, "Invalid timeout message\n");
		return 1;
	}
	if (end_time - start_time < 10 || end_time - start_time > 26) {
		fprintf(stderr, "Unexpected connect timeout (%d)\n", (int) (end_time - start_time));
		return 1;
	}

	odbc_disconnect();

	if (end_socket >= 0)
		close(end_socket);

	printf("Done.\n");
	return 0;
}

int
main(void)
{
	odbc_use_version3 = 1;

	if (Test(0) || Test(1))
		return 1;
	return 0;
}

#else
int
main(void)
{
	printf("Not possible for this platform.\n");
	odbc_test_skipped();
	return 0;
}
#endif
