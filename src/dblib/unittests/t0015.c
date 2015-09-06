/* 
 * Purpose: Test binding and retrieving strings and ints, and cancelling results 
 * Functions: dbbind dbcancel dbnextrow dbnumcols dbresults dbsqlexec 
 */

#include "common.h"

int failed = 0;


int
main(int argc, char **argv)
{
	const int rows_to_add = 50;
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	DBINT testint;

	set_malloc_options();

	read_login_info(argc, argv);
	fprintf(stdout, "Starting %s\n", argv[0]);

	/* Fortify_EnterScope(); */
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0015");

	fprintf(stdout, "About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	fprintf(stdout, "creating table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "insert\n");
	for (i = 0; i < rows_to_add; i++) {
		sql_cmd(dbproc);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}

	fprintf(stdout, "select\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		failed = 1;
		fprintf(stdout, "Was expecting a result set.");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	if (SUCCEED != dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint)) {
		failed = 1;
		fprintf(stderr, "Had problem with bind\n");
		abort();
	}
	if (SUCCEED != dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr)) {
		failed = 1;
		fprintf(stderr, "Had problem with bind\n");
		abort();
	}

	if (REG_ROW != dbnextrow(dbproc)) {
		failed = 1;
		fprintf(stderr, "Failed.  Expected a row\n");
		exit(1);
	}

	dbcancel(dbproc);

	fprintf(stdout, "select 2\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		failed = 1;
		fprintf(stdout, "Was expecting a result set.");
		exit(1);
	}

	if (SUCCEED != dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint)) {
		failed = 1;
		fprintf(stderr, "Had problem with bind\n");
		abort();
	}
	if (SUCCEED != dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr)) {
		failed = 1;
		fprintf(stderr, "Had problem with bind\n");
		abort();
	}

	for (i = 26; i < rows_to_add; i++) {
	char expected[1024];

		sprintf(expected, "row %03d", i);

		if (REG_ROW != dbnextrow(dbproc)) {
			failed = 1;
			fprintf(stderr, "Failed.  Expected a row\n");
			exit(1);
		}
		if (testint != i) {
			failed = 1;
			fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i, (int) testint);
			abort();
		}
		if (0 != strncmp(teststr, expected, strlen(expected))) {
			failed = 1;
			fprintf(stdout, "Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
			abort();
		}
		printf("Read a row of data -> %d %s\n", (int) testint, teststr);
	}

	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		failed = 1;
		fprintf(stderr, "Was expecting no more rows\n");
		exit(1);
	}

	dbexit();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
