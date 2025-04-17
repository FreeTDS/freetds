/* 
 * Purpose: Log in, create a table, insert a few rows, select them, and log out.   
 * Functions: dbbind dbcmd dbcolname dberrhandle dbisopt dbmsghandle dbnextrow dbnumcols dbopen dbresults dbsetlogintime dbsqlexec dbuse 
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
	DBINT testint, erc;

	set_malloc_options();

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	/* Fortify_EnterScope(); */
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon as \"%s\"\n", USER);

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0001");

	printf("About to open \"%s\"\n", SERVER);

	dbproc = dbopen(login, SERVER);
	if (!dbproc) {
		fprintf(stderr, "Unable to connect to %s\n", SERVER);
		return 1;
	}
	dbloginfree(login);  

	printf("Using database \"%s\"\n", DATABASE);
	if (strlen(DATABASE)) {
		erc = dbuse(dbproc, DATABASE);
		assert(erc == SUCCEED);
	}

#ifdef DBQUOTEDIDENT
	printf("QUOTED_IDENTIFIER is %s\n", (dbisopt(dbproc, DBQUOTEDIDENT, NULL))? "ON":"OFF");
#endif
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	for (i = 0; i < rows_to_add && sql_cmd(dbproc) == SUCCEED; i++) {
		dbsqlexec(dbproc);
		while (dbresults(dbproc) == SUCCEED) {
			/* nop */
		}
	}

	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		failed = true;
		fprintf(stderr, "error: expected a result set, none returned.\n");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	if (SUCCEED != dbbind(dbproc, 1, INTBIND, -1, (BYTE *) & testint)) {
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

		memset(teststr, 'x', sizeof(teststr));
		teststr[0] = 0;
		teststr[sizeof(teststr) - 1] = 0;
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
		printf("Read a row of data -> %d |%s|\n", (int) testint, teststr);
	}

	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		failed = true;
		fprintf(stderr, "Was expecting no more rows\n");
		exit(1);
	}

	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
