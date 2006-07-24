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

/*
 * test error on connection close 
 * With a trick we simulate a connection close then we try to 
 * prepare or execute a query. This should fail and return an error message.
 */

static char software_version[] = "$Id: prepclose.c,v 1.3 2006-07-24 09:40:46 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#if HAVE_FSTAT && defined(S_IFSOCK)

static int
close_last_socket(void)
{
	int max_socket = -1, i;
	int sockets[2];

	for (i = 3; i < 1024; ++i) {
		struct stat file_stat;
		if (fstat(i, &file_stat))
			continue;
		if ((file_stat.st_mode & S_IFSOCK) == S_IFSOCK)
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
	SQLRETURN ret;
	unsigned char sqlstate[6];

	Connect();

	if (!close_last_socket()) {
		fprintf(stderr, "Error closing connection\n");
		return 1;
	}

	/* force disconnection closing socket */
	if (direct)
		ret = SQLExecDirect(Statement, (SQLCHAR *) "SELECT 1", SQL_NTS);
	else
		ret = SQLPrepare(Statement, (SQLCHAR *) "SELECT 1", SQL_NTS);
	if (ret != SQL_ERROR) {
		fprintf(stderr, "Error expected\n");
		return 1;
	}

	ret = SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, sqlstate, NULL, (SQLCHAR *) buf, sizeof(buf), NULL);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		fprintf(stderr, "Error not set\n");
		Disconnect();
		return 1;
	}
	sqlstate[5] = 0;
	printf("state=%s err=%s\n", (char*) sqlstate, buf);
	
	Disconnect();

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

