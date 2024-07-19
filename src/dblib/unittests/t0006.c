/* 
 * Purpose: Test fetching 50 rows in two result sets with a 5000-row buffer.
 * Functions: dbbind dbcmd dbnextrow dbnumcols dbresults dbsetopt dbsqlexec
 */

#include "common.h"

static char teststr[1024];
static DBINT testint;

static int failed = 0;


static void
get_results(DBPROCESS * dbproc, int start)
{
	int current = start - 1;

	while (REG_ROW == dbnextrow(dbproc)) {
	char expected[1024];

		current++;
		sprintf(expected, "row %04d", current);

		if (testint != current) {
			fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", current, (int) testint);
			abort();
		}
		if (0 != strncmp(teststr, expected, strlen(expected))) {
			printf("Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
			abort();
		}
		printf("Read a row of data -> %d %s\n", (int) testint, teststr);
	}
}


TEST_MAIN()
{
	RETCODE rc;
	const int rows_to_add = 50;
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;

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
	DBSETLAPP(login, "t0006");
	DBSETLHOST(login, "ntbox.dntis.ro");

	printf("About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

#ifdef MICROSOFT_DBLIB
	dbsetopt(dbproc, DBBUFFER, "5000");
#else
	dbsetopt(dbproc, DBBUFFER, "5000", 0);
#endif

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

	printf("first select\n");
	if (SUCCEED != sql_cmd(dbproc)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}
	if (SUCCEED != dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbsqlexec failed\n", __FILE__, __LINE__);
		failed = 1;
	}


	if (dbresults(dbproc) != SUCCEED) {
		printf("%s:%d: Was expecting a result set.", __FILE__, __LINE__);
		failed = 1;
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
	dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);

	get_results(dbproc, 1);

	testint = -1;
	strcpy(teststr, "bogus");
	printf("second select\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if ((rc = dbresults(dbproc)) != SUCCEED) {
		printf("%s:%d: Was expecting a result set. (rc=%d)\n", __FILE__, __LINE__, rc);
		failed = 1;
	}

	if (!failed) {
		dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
		dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);

		get_results(dbproc, 25);
	}
	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
