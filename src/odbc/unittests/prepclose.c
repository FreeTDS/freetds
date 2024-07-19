#include "common.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

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

#include <freetds/utils.h>

/*
 * test error on connection close 
 * With a trick we simulate a connection close then we try to 
 * prepare or execute a query. This should fail and return an error message.
 */

#ifdef _WIN32
#define SHUT_RDWR SD_BOTH
#endif

static int
close_last_socket(void)
{
	TDS_SYS_SOCKET max_socket = odbc_find_last_socket();

	if (max_socket < 0) {
		fprintf(stderr, "Error finding last socket\n");
		return 0;
	}

	/* close connection */
	shutdown(max_socket, SHUT_RDWR);

	return 1;
}

static int
Test(int direct, int long_query)
{
	SQLTCHAR buf[256];
	SQLTCHAR sqlstate[6];
	char sql[5100];

	odbc_mark_sockets_opened();
	odbc_connect();

	if (!close_last_socket()) {
		fprintf(stderr, "Error closing connection\n");
		return 1;
	}

	if (long_query) {
		memset(sql, '-', sizeof(sql));
		strcpy(sql + 5000, "\nSELECT 1");
	} else {
		strcpy(sql, "SELECT 1");
	}

	/* force disconnection closing socket */
	if (direct) {
		CHKExecDirect(T(sql), SQL_NTS, "E");
	} else {
		SQLSMALLINT cols;
		/* use prepare, force dialog with server */
		if (CHKPrepare(T(sql), SQL_NTS, "SE") == SQL_SUCCESS)
			CHKNumResultCols(&cols, "E");
	}

	CHKGetDiagRec(SQL_HANDLE_STMT, odbc_stmt, 1, sqlstate, NULL, buf, TDS_VECTOR_SIZE(buf), NULL, "SI");
	sqlstate[5] = 0;
	printf("state=%s err=%s\n", C(sqlstate), C(buf));
	
	odbc_disconnect();

	printf("Done.\n");
	return 0;
}

TEST_MAIN()
{
	/* check with short queries */
	if (Test(0, 0) || Test(1, 0))
		return 1;

	/* check with large queries, this will trigger different paths */
	if (Test(0, 1) || Test(1, 1))
		return 1;
	return 0;
}
