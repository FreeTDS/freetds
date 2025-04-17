/* 
 * Purpose: Test behaviour of DBCOUNT() for updates and inserts
 * Functions: DBCOUNT dbbind dbnextrow dbnumcols dbopen dbresults dbsqlexec 
 */

#include "common.h"

#include <freetds/bool.h>

TEST_MAIN()
{
	bool failed = false;
	const int rows_to_add = 50;
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	DBINT testint;

	set_malloc_options();

	read_login_info(argc, argv);
	printf("Starting %s\n", argv[0]);

	/* Fortify_EnterScope(); */
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0018");

	printf("About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	printf("creating table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	printf("insert\n");
	for (i = 0; i < rows_to_add; i++) {
		sql_cmd(dbproc);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
		if (DBCOUNT(dbproc) != 1) {
			failed = true;
			printf("Was expecting a rows affect to be 1.");
			exit(1);
		}
	}

	printf("select\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	printf("Checking for an empty result set.\n");
	if (dbresults(dbproc) != SUCCEED) {
		failed = true;
		printf("Was expecting a result set.\n");
		exit(1);
	}

	if(DBROWS(dbproc) != FAIL) {
		failed = true;
		printf("Was expecting no rows to be available.\n");
		exit(1);
	}

	printf("Checking for a result set with content.\n");
	if (dbresults(dbproc) != SUCCEED) {
		failed = true;
		printf("Was expecting a result set.");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	if (SUCCEED != dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint)) {
		failed = true;
		fprintf(stderr, "Had problem with bind\n");
		abort();
	}
	if (SUCCEED != dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr)) {
		failed = true;
		fprintf(stderr, "Had problem with bind\n");
		abort();
	}

	for (i = 0; i < rows_to_add; i++) {
	char expected[1024];

		sprintf(expected, "row %03d", i);

		if (REG_ROW != dbnextrow(dbproc)) {
			failed = true;
			fprintf(stderr, "Failed.  Expected a row\n");
			exit(1);
		}
		if (testint != i) {
			failed = true;
			fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i, (int) testint);
			abort();
		}
		if (0 != strncmp(teststr, expected, strlen(expected))) {
			failed = true;
			printf("Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
			abort();
		}
		printf("Read a row of data -> %d %s\n", (int) testint, teststr);
	}

	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		failed = true;
		fprintf(stderr, "Was expecting no more rows\n");
		exit(1);
	}
	if (DBCOUNT(dbproc) != rows_to_add) {
		failed = true;
		printf("Was expecting a rows affect to be %d was %d.\n", rows_to_add, DBCOUNT(dbproc));
		exit(1);
	}

	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	if (DBCOUNT(dbproc) != rows_to_add) {
		failed = true;
		printf("Was expecting a rows affect to be %d was %d.\n", rows_to_add, DBCOUNT(dbproc));
		exit(1);
	} else {
		printf("Number of rows affected by update = %d.\n", DBCOUNT(dbproc));
	}

	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
