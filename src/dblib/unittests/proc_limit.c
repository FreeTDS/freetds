/*
 * Purpose: Test reaching processes limit trigger a specific error
 * Functions: dbopen dbsetmaxprocs
 */

#include "common.h"
#include <freetds/bool.h>

static bool proc_limit_hit = false;

static int
err_handler(DBPROCESS * dbproc TDS_UNUSED, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
	if (dberr == 20011) {
		proc_limit_hit = true;
		fprintf(stderr, "OK: anticipated error %d (%s) arrived\n", dberr, dberrstr);
		return INT_CANCEL;
	}

	fprintf(stderr,
		"DB-LIBRARY error (severity %d, dberr %d, oserr %d, dberrstr %s, oserrstr %s):\n",
		severity, dberr, oserr, dberrstr ? dberrstr : "(null)", oserrstr ? oserrstr : "(null)");
	fflush(stderr);

	return INT_CANCEL;
}

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc, *dbproc2;
	bool failed = false;

	set_malloc_options();

	read_login_info(argc, argv);
	printf("Starting %s\n", argv[0]);
	dbinit();

	/* limit number of processes to get error */
	dbsetmaxprocs(1);

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "proc_limit");

	dbproc = dbopen(login, SERVER);

	/* another process should fail as limit reached */
	dberrhandle(err_handler);
	dbproc2 = dbopen(login, SERVER);
	dbloginfree(login);

	if (dbproc == NULL || dbproc2 != NULL || !proc_limit_hit)
		failed = true;

	dbclose(dbproc);
	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
