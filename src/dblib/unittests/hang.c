/* 
 * Purpose: Test for dbsqlexec on closed connection
 */

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

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NET_INET_IN_H */

#include <freetds/utils.h>

#if HAVE_FSTAT && defined(S_IFSOCK)

static char *UNITTEST;
static int end_socket = -1;

static int
shutdown_socket(DBPROCESS *dbproc)
{
	union {
		struct sockaddr sa;
		char data[256];
	} u;
	SOCKLEN_T addrlen;
	struct stat file_stat;
	TDS_SYS_SOCKET sockets[2];

	TDS_SYS_SOCKET socket = DBIOWDESC(dbproc);

	if (fstat(socket, &file_stat))
		return 0;
	if ((file_stat.st_mode & S_IFSOCK) != S_IFSOCK)
		return 0;

	addrlen = sizeof(u);
	if (tds_getsockname(socket, &u.sa, &addrlen) < 0 || (u.sa.sa_family != AF_INET && u.sa.sa_family != AF_INET6))
		return 0;

	/* replace socket with a new one */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
		return 0;

	tds_socket_set_nosigpipe(sockets[0], 1);

	/* substitute socket */
	close(socket);
	dup2(sockets[0], socket);

	/* close connection */
	close(sockets[0]);
	end_socket = sockets[1];
	return 1;
}

static int
test(int close_socket)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	RETCODE ret;
	int expected_error = -1;

	printf("Starting %s\n", UNITTEST);

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "hang");

	printf("About to open\n");

	dbproc = dbopen(login, SERVER);
	dbsetuserdata(dbproc, (BYTE*) &expected_error);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	dbcmd(dbproc, "select * from sysobjects");
	printf("dbsqlexec should not hang.\n");

	ret = dbsettime(15);
	if (ret != SUCCEED) {
		fprintf(stderr, "Failed.  Error setting timeout.\n");
		return 1;
	}

	if (!shutdown_socket(dbproc)) {
		fprintf(stderr, "Error shutting down connection\n");
		return 1;
	}
	if (close_socket)
		close(end_socket);

	alarm(20);
	expected_error = close_socket ? 20006 : 20003;
	ret = dbsqlexec(dbproc);
	alarm(0);
	if (ret != FAIL) {
		fprintf(stderr, "Failed.  Expected FAIL to be returned.\n");
		return 1;
	}

	dbsetuserdata(dbproc, NULL);
	if (!close_socket)
		close(end_socket);
	dbexit();

	printf("dblib okay on %s\n", __FILE__);
	return 0;
}

TEST_MAIN()
{
	UNITTEST = argv[0];
	read_login_info(argc, argv);
	if (test(0) || test(1))
		return 1;
	return 0;
}

#else
TEST_MAIN()
{
	fprintf(stderr, "Not possible for this platform.\n");
	return 0;
}
#endif

