/* 
 * Purpose: Test retrieving data and attempting to initiate a new query with results pending
 * Functions: dbnextrow dbresults dbsqlexec dbgetchar 
 */

#include "common.h"
#include <ctype.h>

TEST_MAIN()
{
	const int rows_to_add = 50;
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i, expected_error;
	char *s, teststr[1024];
	DBINT testint;
	int failed = 0;

	set_malloc_options();

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0004");

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
	for (i = 1; i < rows_to_add; i++) {
		sql_cmd(dbproc);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}

	sql_cmd(dbproc); /* select */
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		printf("Was expecting a result set.");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
	dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);

	for (i = 1; i <= 24; i++) {
	char expected[1024];

		sprintf(expected, "row %04d", i);

		if (i % 5 == 0) {
			dbclrbuf(dbproc, 5);
		}


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
			printf("Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
			abort();
		}
		printf("Read a row of data -> %d %s\n", (int) testint, teststr);
	}



	printf("second select\n");
	printf("testing dbgetchar...\n");
	for (i=0; (s = dbgetchar(dbproc, i)) != NULL; i++) {
		putchar(*s);
		if (!(isprint((unsigned char)*s) || iscntrl((unsigned char)*s))) {
			fprintf(stderr, "%s:%d: dbgetchar failed with %x at position %d\n", __FILE__, __LINE__, *s, i);
			failed = 1;
			break;
		}
	}
	printf("<== end of command buffer\n");

	if (SUCCEED != sql_cmd(dbproc)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}
	printf("About to exec for the second time.  Should fail\n");
	expected_error = 20019;
	dbsetuserdata(dbproc, (BYTE*) &expected_error);
	if (FAIL != dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbsqlexec should have failed but didn't\n", __FILE__, __LINE__);
		failed = 1;
	}
	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
