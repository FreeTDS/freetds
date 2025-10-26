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

/* Test for SQLEndTran */

static void
ReadErrorConn(void)
{
	ODBC_BUF *odbc_buf = NULL;
	SQLTCHAR *err = (SQLTCHAR *) ODBC_GET(sizeof(odbc_err)*sizeof(SQLTCHAR));
	SQLTCHAR *state = (SQLTCHAR *) ODBC_GET(sizeof(odbc_sqlstate)*sizeof(SQLTCHAR));

	memset(odbc_err, 0, sizeof(odbc_err));
	memset(odbc_sqlstate, 0, sizeof(odbc_sqlstate));
	CHKGetDiagRec(SQL_HANDLE_DBC, odbc_conn, 1, state, NULL, err, sizeof(odbc_err), NULL, "SI");
	strcpy(odbc_err, C(err));
	strcpy(odbc_sqlstate, C(state));
	ODBC_FREE();
	printf("Message: '%s' %s\n", odbc_sqlstate, odbc_err);
}

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

TEST_MAIN()
{
	odbc_use_version3 = true;

	odbc_mark_sockets_opened();
	odbc_connect();

	CHKSetConnectAttr(SQL_ATTR_AUTOCOMMIT, (void *) SQL_AUTOCOMMIT_OFF, 0, "S");

	odbc_command("SELECT 1");
	CHKMoreResults("No");

	if (!close_last_socket()) {
		fprintf(stderr, "Error closing connection\n");
		return 1;
	}
	CHKEndTran(SQL_HANDLE_DBC, odbc_conn, SQL_ROLLBACK, "E");

	/* the error should be written in the connection, not in the statement */
	ReadErrorConn();
	if (strcmp(odbc_sqlstate, "08S01") != 0 || strstr(odbc_err, "Write to the server") == NULL) {
		odbc_disconnect();
		fprintf(stderr, "Unexpected error message %s %s\n", odbc_sqlstate, odbc_err);
		return 1;
	}

	odbc_disconnect();
	return 0;
}
