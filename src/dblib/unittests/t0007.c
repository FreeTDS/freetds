/* 
 * Purpose: Test binding strings and ints, attempt 2nd query with results pending. 
 * Functions: dbbind dbcmd dbnextrow dbopen dbresults dbsqlexec 
 */

#include "common.h"

static void
create_tables(DBPROCESS * dbproc, int rows_to_add)
{
	int i;

	fprintf(stdout, "creating table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
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
}


static int
start_query(DBPROCESS * dbproc)
{
	int i;

	if (SUCCEED != sql_cmd(dbproc)) {
		return 0;
	}
	if (SUCCEED != dbsqlexec(dbproc)) {
		return 0;
	}

	if (dbresults(dbproc) != SUCCEED)
		return 0;

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is named \"%s\"\n", i, dbcolname(dbproc, i));

	return 1;
}

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	DBINT testint;
	DBVARYBIN  testvbin;
	DBVARYCHAR testvstr;
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
	DBSETLAPP(login, "t0007");

	fprintf(stdout, "About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	create_tables(dbproc, 10);

	if (!start_query(dbproc)) {
		fprintf(stderr, "%s:%d: start_query failed\n", __FILE__, __LINE__);
		failed = 1;
	}

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
	dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);

	for (i = 1; i <= 2; i++) {
		char expected[1024];

		sprintf(expected, "row %07d", i);

		if (i % 5 == 0) {
			dbclrbuf(dbproc, 5);
		}

		testint = -1;
		strcpy(teststr, "bogus");

		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			abort();
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


	fprintf(stdout, "second select.  Should fail.\n");

	expected_error = 20019;
	dbsetuserdata(dbproc, (BYTE*) &expected_error);

	if (start_query(dbproc)) {
		fprintf(stderr, "%s:%d: start_query should have failed but didn't\n", __FILE__, __LINE__);
		failed = 1;
	}

	dbcancel(dbproc);
	
	/* 
	 * Test Binary binding
	 */
	if (!start_query(dbproc)) {
		fprintf(stderr, "%s:%d: start_query failed\n", __FILE__, __LINE__);
		failed = 1;
	}

	dbbind(dbproc, 1, VARYBINBIND, sizeof(testvbin), (BYTE *) &testvbin);
	dbbind(dbproc, 2, VARYCHARBIND, sizeof(testvstr), (BYTE *) &testvstr);
	dbbind(dbproc, 3, BINARYBIND, sizeof(testint), (BYTE *) &testint);

	for (i = 1; i <= 2; i++) {
		char expected[1024];

		sprintf(expected, "row %07d ", i);

		testint = -1;
		memset(&testvbin, '*', sizeof(testvbin));
		memset(&testvstr, '*', sizeof(testvstr));

		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			abort();
		}
		if (testint != i) {
			fprintf(stderr, "Failed, line %d.  Expected i to be %d, was %d (0x%x)\n", __LINE__, i, (int) testint, (int) testint);
			abort();
		}
		if (testvbin.len != sizeof(testint)) {
			fprintf(stderr, "Failed, line %d.  Expected bin lenght to be %d, was %d\n", __LINE__, (int) sizeof(testint), (int) testvbin.len);
			abort();
		}
		memcpy(&testint, testvbin.array, sizeof(testint));
		if (testint != i) {
			fprintf(stderr, "Failed, line %d.  Expected i to be %d, was %d (0x%x)\n", __LINE__, i, (int) testint, (int) testint);
			abort();
		}
		if (testvstr.len != strlen(expected) || 0 != strncmp(testvstr.str, expected, strlen(expected))) {
			fprintf(stdout, "Failed, line %d.  Expected s to be |%s|, was |%s|\n", __LINE__, expected, testvstr.str);
			abort();
		}
		testvstr.str[testvstr.len] = 0;
		printf("Read a row of data -> %d %s\n", (int) testint, testvstr.str);
	}


	dbexit();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
