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

#ifndef _WIN32
#include "tds_sysdep_private.h"
#endif

/*
 * test error on connection close 
 * With a trick we simulate a connection close then we try to 
 * prepare or execute a query. This should fail and return an error message.
 */

static char software_version[] = "$Id: prepclose.c,v 1.7 2010-07-05 09:20:33 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#if HAVE_FSTAT && defined(S_IFSOCK)

static int
close_last_socket(void)
{
	int max_socket = -1, i;
	int sockets[2];

	for (i = 3; i < 1024; ++i) {
		struct stat file_stat;
		union {
			struct sockaddr sa;
			char data[256];
		} u;
		SOCKLEN_T addrlen;

		if (fstat(i, &file_stat))
			continue;
		if ((file_stat.st_mode & S_IFSOCK) != S_IFSOCK)
			continue;
		addrlen = sizeof(u);
		if (tds_getsockname(i, &u.sa, &addrlen) < 0)
			continue;
		if (u.sa.sa_family != AF_INET && u.sa.sa_family != AF_INET6)
			continue;
		max_socket = i;
	}
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
	close(sockets[1]);
	return 1;
}

static int
Test(int direct)
{
	char buf[256];
	unsigned char sqlstate[6];

	odbc_connect();

	if (!close_last_socket()) {
		fprintf(stderr, "Error closing connection\n");
		return 1;
	}

	/* force disconnection closing socket */
	if (direct) {
		CHKExecDirect((SQLCHAR *) "SELECT 1", SQL_NTS, "E");
	} else {
		SQLSMALLINT cols;
		/* use prepare, force dialog with server */
		if (CHKPrepare((SQLCHAR *) "SELECT 1", SQL_NTS, "SE") == SQL_SUCCESS)
			CHKNumResultCols(&cols, "E");
	}

	CHKGetDiagRec(SQL_HANDLE_STMT, odbc_stmt, 1, sqlstate, NULL, (SQLCHAR *) buf, sizeof(buf), NULL, "SI");
	sqlstate[5] = 0;
	printf("state=%s err=%s\n", (char*) sqlstate, buf);
	
	odbc_disconnect();

	printf("Done.\n");
	return 0;
}

int
main(void)
{
	if (Test(0) || Test(1))
		return 1;
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

