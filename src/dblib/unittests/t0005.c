/* 
 * Purpose: Test data retrieval accuracy and cancelling results
 * Functions: dbbind dbcancel dbcmd dbcolname dbnextrow dbnumcols dbresults dbsqlexec 
 */

#include "common.h"

int
main(int argc, char **argv)
{
	const int rows_to_add = 50;
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	DBINT testint;
	int failed = 0;
	int expected_error;

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
	DBSETLAPP(login, "t0005");
	DBSETLHOST(login, "ntbox.dntis.ro");

	fprintf(stdout, "About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	fprintf(stdout, "creating table\n");
	if (SUCCEED != sql_cmd(dbproc)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}

	if (SUCCEED != dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}

	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "insert\n");
	for (i = 1; i < rows_to_add; i++) {
		sql_cmd(dbproc);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}


	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stdout, "Was expecting a result set.");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
	dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);

	for (i = 1; i < 5; i++) {
	char expected[1024];

		sprintf(expected, "row %04d", i);

		testint = -1;
		strcpy(teststr, "bogus");

		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			exit(1);
		}
		if (testint != i) {
			fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i, (int) testint);
			abort();
		}
		if (0 != strncmp(teststr, expected, strlen(expected))) {
			fprintf(stdout, "Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
			abort();
		}
		printf("Read a row of data -> %d %s\n", (int) testint, teststr);
	}

	fprintf(stdout, "This query should succeeded as we have fetched the exact\n"
			"number of rows in the result set\n");

	expected_error = 20019;
	dbsetuserdata(dbproc, (BYTE*) &expected_error);

	if (SUCCEED != sql_cmd(dbproc)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}
	if (SUCCEED != dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbsqlexec should have succeeded but didn't\n", __FILE__, __LINE__);
		failed = 1;
	}

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stdout, "Was expecting a result set.");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
	dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);

	for (i = 1; i < 5; i++) {
	char expected[1024];

		sprintf(expected, "row %04d", i);

		testint = -1;
		strcpy(teststr, "bogus");

		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			exit(1);
		}
		if (testint != i) {
			fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i, (int) testint);
			abort();
		}
		if (0 != strncmp(teststr, expected, strlen(expected))) {
			fprintf(stdout, "Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
			abort();
		}
		printf("Read a row of data -> %d %s\n", (int) testint, teststr);
	}

	fprintf(stdout, "Testing anticipated failure\n");

	if (SUCCEED != sql_cmd(dbproc)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}
	if (SUCCEED == dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbsqlexec should have failed but didn't\n", __FILE__, __LINE__);
		failed = 1;
	}


	fprintf(stdout, "calling dbcancel to flush results\n");
	dbcancel(dbproc);

	fprintf(stdout, "Dropping proc\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "creating proc\n");
	sql_cmd(dbproc);
	if (dbsqlexec(dbproc) == FAIL) {
		fprintf(stderr, "%s:%d: failed to create procedure\n", __FILE__, __LINE__);
		failed = 1;
	}
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "calling proc\n");
	sql_cmd(dbproc);
	if (dbsqlexec(dbproc) == FAIL) {
		fprintf(stderr, "%s:%d: failed to call procedure\n", __FILE__, __LINE__);
		failed = 1;
	}
	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stdout, "Was expecting a result set.");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
	dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);

	for (i = 1; i < 6; i++) {
	char expected[1024];

		sprintf(expected, "row %04d", i);

		testint = -1;
		strcpy(teststr, "bogus");

		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			exit(1);
		}
		if (testint != i) {
			fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i, (int) testint);
			abort();
		}
		if (0 != strncmp(teststr, expected, strlen(expected))) {
			fprintf(stdout, "Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
			abort();
		}
		printf("Read a row of data -> %d %s\n", (int) testint, teststr);
	}

	fprintf(stdout, "Calling dbsqlexec before dbnextrow returns NO_MORE_ROWS\n");
	fprintf(stdout, "The following command should succeed because\n"
			"we have fetched the exact number of rows in the result set\n");

	if (SUCCEED != sql_cmd(dbproc)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}
	if (SUCCEED != dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbsqlexec should have succeeded but didn't\n", __FILE__, __LINE__);
		failed = 1;
	}

	dbcancel(dbproc);

	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	dbexit();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
