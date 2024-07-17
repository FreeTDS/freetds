/* 
 * Purpose: Test NULL behavior in order to fix problems with PHP and NULLs
 * PHP use dbdata to get data
 */

#include "common.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifndef DBNTWIN32

static DBPROCESS *dbproc = NULL;
static int failed = 0;

static int
ignore_msg_handler(DBPROCESS * dbproc TDS_UNUSED, DBINT msgno TDS_UNUSED, int state TDS_UNUSED, int severity TDS_UNUSED,
		   char *text TDS_UNUSED, char *server TDS_UNUSED, char *proc TDS_UNUSED, int line TDS_UNUSED)
{
	return 0;
}

static int
ignore_err_handler(DBPROCESS * dbproc TDS_UNUSED, int severity TDS_UNUSED, int dberr TDS_UNUSED,
		   int oserr TDS_UNUSED, char *dberrstr TDS_UNUSED, char *oserrstr TDS_UNUSED)
{
	return INT_CANCEL;
}

static void
query(const char *query)
{
	printf("query: %s\n", query);
	dbcmd(dbproc, (char *) query);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}
}

static void
test0(int n, int expected)
{
	static const char sql[] = "select c from #null where n = %d";

	printf(sql, n);
	printf(" ... ");
	
	dbfcmd(dbproc, sql, n);

	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED || dbnextrow(dbproc) != REG_ROW) {
		printf("\nExpected a row.\n");
		failed = 1;
		dbcancel(dbproc);
		return;
	}

	printf("got %p and length %d\n", dbdata(dbproc, 1), dbdatlen(dbproc, 1));

	if (dbdatlen(dbproc, 1) != (expected < 0? 0 : expected)) {
		fprintf(stderr, "Error: n=%d: dbdatlen returned %d, expected %d\n", 
				n, dbdatlen(dbproc, 1), expected < 0? 0 : expected);
		dbcancel(dbproc);
		failed = 1;
	}

	if (dbdata(dbproc, 1) != NULL && expected  < 0) {
		fprintf(stderr, "Error: n=%d: dbdata returned %p, expected NULL and length %d\n", 
				n, dbdata(dbproc, 1), expected);
		dbcancel(dbproc);
		failed = 1;
	}

	if (dbdata(dbproc, 1) == NULL && expected  > 0) {
		fprintf(stderr, "Error: n=%d: dbdata returned %p, expected non-NULL and length %d\n", 
				n, dbdata(dbproc, 1), expected);
		dbcancel(dbproc);
		failed = 1;
	}

	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		fprintf(stderr, "Error: Only one row expected (cancelling remaining results)\n");
		dbcancel(dbproc);
		failed = 1;
	}
	


	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}
}


static void
test(const char *type, int give_err)
{
	RETCODE ret;

	query("if object_id('#null') is not NULL drop table #null");

	dberrhandle(ignore_err_handler);
	dbmsghandle(ignore_msg_handler);

	printf("create table #null (n int, c %s NULL)\n", type);
	dbfcmd(dbproc, "create table #null (n int, c %s NULL)", type);
	dbsqlexec(dbproc);

	ret = dbresults(dbproc);

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	if (ret != SUCCEED) {
		dbcancel(dbproc);
		if (!give_err)
			return;
		printf("Was expecting a result set.\n");
		failed = 1;
		return;
	}

	query("insert into #null values(1, '')");
	query("insert into #null values(2, NULL)");
	query("insert into #null values(3, ' ')");
	query("insert into #null values(4, 'a')");

	test0(1, DBTDS_5_0 < DBTDS(dbproc)?  0 : 1);
	test0(2, DBTDS_5_0 < DBTDS(dbproc)? -1 : 0);
	test0(3, 1);
	test0(4, 1);

	query("drop table #null");
}

TEST_MAIN()
{
	LOGINREC *login;

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "null");

	printf("About to open \"%s\"\n", SERVER);

	dbproc = dbopen(login, SERVER);
	if (!dbproc) {
		fprintf(stderr, "Unable to connect to %s\n", SERVER);
		return 1;
	}

	dbloginfree(login);

	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);

	test("VARCHAR(10)", 1);
	test("TEXT", 1);

	test("NVARCHAR(10)", 0);
	if (DBTDS_5_0 < DBTDS(dbproc)) {
		test("NTEXT", 0);
		test("VARCHAR(MAX)", 0);
		test("NVARCHAR(MAX)", 0);
	}

	dbexit();

	return failed ? 1 : 0;
}
#else
TEST_MAIN()
{
	fprintf(stderr, "Not supported by MS DBLib\n");
	return 0;
}
#endif

