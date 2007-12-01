/* 
 * Purpose: Test NULL behavior in order to fix problems with PHP and NULLs
 * PHP use dbdata to get data
 */

#include "common.h"

#include <unistd.h>

static char software_version[] = "$Id: null.c,v 1.4 2007-12-01 19:24:03 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static DBPROCESS *dbproc = NULL;
static int failed = 0;

static int
ignore_msg_handler(DBPROCESS * dbproc, DBINT msgno, int state, int severity, char *text, char *server, char *proc, int line)
{
	int res;

	dbsetuserdata(dbproc, (BYTE*) &msgno);
	res = syb_msg_handler(dbproc, msgno, state, severity, text, server, proc, line);
	dbsetuserdata(dbproc, NULL);
	return res;
}

static int
ignore_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
	int res;

	dbsetuserdata(dbproc, (BYTE*) &dberr);
	res = syb_err_handler(dbproc, severity, dberr, oserr, dberrstr, oserrstr);
	dbsetuserdata(dbproc, NULL);
	return res;
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
test0(int n, int len)
{
	dbfcmd(dbproc, "select c from #null where n = %d", n);

	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED || dbnextrow(dbproc) != REG_ROW) {
		fprintf(stdout, "Was expecting a row.\n");
		failed = 1;
		dbcancel(dbproc);
		return;
	}

	if (dbdatlen(dbproc, 1) != (len < 0 ? 0 : len) || 
	    (len  < 0 && dbdata(dbproc, 1) != NULL) || 
	    (len >= 0 && dbdata(dbproc, 1) == NULL)) {
		fprintf(stderr, "Error: unexpected result for n == %d len %d data %p\n", n, dbdatlen(dbproc, 1), dbdata(dbproc, 1));
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

	dberrhandle(ignore_err_handler);
	dbmsghandle(ignore_msg_handler);

	query("if object_id('#null') is not NULL drop table #null");

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
		fprintf(stdout, "Was expecting a result set.\n");
		failed = 1;
		return;
	}

	query("insert into #null values(1, '')");
	query("insert into #null values(2, NULL)");
	query("insert into #null values(3, ' ')");
	query("insert into #null values(4, 'a')");

	test0(1, 0);
	test0(2, -1);
	test0(3, 1);
	test0(4, 1);

	query("drop table #null");
}

int
main(int argc, char **argv)
{
	LOGINREC *login;

	read_login_info(argc, argv);

	fprintf(stdout, "Start\n");

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "thread");

	fprintf(stdout, "About to open \"%s\"\n", SERVER);

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
	test("NTEXT", 0);

	test("VARCHAR(MAX)", 0);
	test("NVARCHAR(MAX)", 0);

	dbexit();

	return failed ? 1 : 0;
}

