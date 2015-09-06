/* 
 * Purpose: Test behaviour of DBCOUNT() for updates and inserts
 * Functions: DBCOUNT dbbind dbnextrow dbnumcols dbopen dbresults dbsqlexec 
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
	DBSETLAPP(login, "t0018");

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
		if (DBCOUNT(dbproc) != 1) {
			failed = 1;
			fprintf(stdout, "Was expecting a rows affect to be 1.");
			exit(1);
		}
	}

	fprintf(stdout, "select\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	fprintf(stdout, "Checking for an empty result set.\n");
	if (dbresults(dbproc) != SUCCEED) {
		failed = 1;
		fprintf(stdout, "Was expecting a result set.\n");
		exit(1);
	}

	if(DBROWS(dbproc) != FAIL) {
		failed = 1;
		fprintf(stdout, "Was expecting no rows to be available.\n");
		exit(1);
	}

	fprintf(stdout, "Checking for a result set with content.\n");
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

	for (i = 0; i < rows_to_add; i++) {
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
	if (DBCOUNT(dbproc) != rows_to_add) {
		failed = 1;
		fprintf(stdout, "Was expecting a rows affect to be %d was %d.\n", rows_to_add, DBCOUNT(dbproc));
		exit(1);
	}

	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	if (DBCOUNT(dbproc) != rows_to_add) {
		failed = 1;
		fprintf(stdout, "Was expecting a rows affect to be %d was %d.\n", rows_to_add, DBCOUNT(dbproc));
		exit(1);
	} else {
		fprintf(stdout, "Number of rows affected by update = %d.\n", DBCOUNT(dbproc));
	}

	dbexit();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
