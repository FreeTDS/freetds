/*
 * Purpose: Test check dbcolinfo/dbtableinfo information
 * Functions: dbresults dbsqlexec dbtablecolinfo
 */

#include "common.h"

static int failed = 0;

static void
check_is(const char *value, const char *expected)
{
	if (strcmp(value, expected) == 0)
		return;

	failed = 1;
	fprintf(stderr, "Wrong value, got \"%s\" expected \"%s\"\n", value, expected);
}

static void
check_contains(const char *value, const char *expected)
{
	if (strstr(value, expected) != NULL)
		return;

	failed = 1;
	fprintf(stderr, "Wrong value, got \"%s\" expected to contains \"%s\"\n", value, expected);
}

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int n_col;
	DBCOL2 col2;

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
	DBSETLAPP(login, "colinfo");

	printf("About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	printf("creating tables\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	sql_cmd(dbproc); /* select */
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		printf("Was expecting a result set.");
		exit(1);
	}

	for (n_col = 1; n_col <= 3; ++n_col) {
		col2.SizeOfStruct = sizeof(col2);
		if (dbtablecolinfo(dbproc, n_col, (DBCOL *) &col2) != SUCCEED) {
			fprintf(stderr, "dbtablecolinfo failed for col %d\n", n_col);
			failed = 1;
			continue;
		}

		if (n_col == 1) {
			check_is(col2.Name, "number");
			check_is(col2.ActualName, "is_an_int");
			check_contains(col2.TableName, "#colinfo_table");
		} else if (n_col == 2) {
			check_is(col2.Name, "is_a_string");
			check_is(col2.ActualName, "is_a_string");
			check_contains(col2.TableName, "#colinfo_table");
		} else if (n_col == 3) {
			check_is(col2.Name, "dollars");
			check_is(col2.ActualName, "is_a_money");
			check_contains(col2.TableName, "#test_table");
		}
	}

	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
