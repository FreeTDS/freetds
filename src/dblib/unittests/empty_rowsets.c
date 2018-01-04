/*
 * Purpose: Test handling of empty rowsets
 * Functions: dbcmd dbdata dbdatlen dbnextrow dbresults dbsqlexec
 */

#include "common.h"

static int failed = 0;
static void set_failed(int line)
{
	failed = 1;
	fprintf(stderr, "Failed check at line %d\n", line);
}
#define set_failed() set_failed(__LINE__)

int
main(int argc, char *argv[])
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int ret_code;
	int num_cols;
	int num_res;

	set_malloc_options();

	read_login_info(argc, argv);
	fprintf(stdout, "Starting %s\n", argv[0]);
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0012");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE)) {
		dbuse(dbproc, DATABASE);
	}
	dbloginfree(login);
	fprintf(stdout, "After logon\n");

	/* select */
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	num_res = 0;
	while ((ret_code = dbresults(dbproc)) == SUCCEED) {
		num_cols = dbnumcols(dbproc);
		fprintf(stdout, "Result %d has %d columns\n", num_res, num_cols);
		if (!(num_res % 2) && num_cols)
			set_failed();
		while(dbnextrow(dbproc) != NO_MORE_ROWS) {};
		num_res++;
	}
	if (ret_code == FAIL)
		set_failed();

	dbclose(dbproc);
	dbexit();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
