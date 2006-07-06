/* 
 * Purpose: Test for proper return code from dbsqlexec()
 * Functions: db_name dbcmd dberrhandle dbmsghandle dbnextrow dbopen dbresults dbsqlexec
 */

#include "common.h"

static char software_version[] = "$Id: t0020.c,v 1.14 2006-07-06 12:48:16 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };



int failed = 0;


int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	RETCODE ret;

	set_malloc_options();

	read_login_info(argc, argv);

	fprintf(stdout, "Start\n");
	add_bread_crumb();

	/* Fortify_EnterScope(); */
	dbinit();

	add_bread_crumb();
	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	add_bread_crumb();
	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0020");

	fprintf(stdout, "About to open\n");

	add_bread_crumb();
	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	add_bread_crumb();
	dbloginfree(login);
	add_bread_crumb();

	dbcmd(dbproc, "select dsjfkl dsjf");
	fprintf(stderr, "The following invalid column error is normal.\n");
	ret = dbsqlexec(dbproc);
	if (ret != FAIL) {
		failed = 1;
		fprintf(stderr, "Failed.  Expected FAIL to be returned.\n");
		exit(1);
	}

	dbcmd(dbproc, "select db_name()");
	ret = dbsqlexec(dbproc);
	if (ret != SUCCEED) {
		failed = 1;
		fprintf(stderr, "Failed.  Expected SUCCEED to be returned.\n");
		exit(1);
	}

	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		while (dbnextrow(dbproc) != NO_MORE_ROWS);
	}

	add_bread_crumb();
	dbexit();
	add_bread_crumb();

	fprintf(stdout, "dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	free_bread_crumb();
	return failed ? 1 : 0;
}
