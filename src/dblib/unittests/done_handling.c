#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sybfront.h>
#include <sybdb.h>

#include "common.h"

static char software_version[] = "$Id: done_handling.c,v 1.2 2005-08-31 15:20:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/*
 * This test try do discovery how dblib process token looking for state
 * at every iteration. It issue a query to server and check
 * - row count (valid number means DONE processed)
 * - if possible to send another query (means state IDLE)
 * - if error readed (means ERROR token readed)
 * - if status present (PARAMRESULT token readed)
 * - if parameter prosent (PARAM token readed)
 * If try these query types:
 * - normal row
 * - normal row with no count
 * - normal row without rows
 * - error query
 * - store procedure call with output parameters
 */

/* Forward declarations of the error handler and message handler. */
static int err_handler(DBPROCESS *dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
static int msg_handler(DBPROCESS *dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line);

static DBPROCESS *dbproc;
static int silent = 0;
static int check_idle = 0;

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
check_state(void)
{
	printf("State ");
	if (dbnumcols(dbproc) > 0)
		printf("COLS(%d) ", dbnumcols(dbproc));
 	// row count
	if (dbcount(dbproc) >= 0)
		printf("ROWS(%d) ", (int) dbcount(dbproc));
	// if status present
	if (dbretstatus(dbproc) == TRUE)
		printf("STATUS %d ", (int) dbretstatus(dbproc));
	// if parameter prosent
	if (dbnumrets(dbproc) > 0)
		printf("PARAMS ");
	// if possible to send another query
	// NOTE this must be the last
	if (check_idle) {
		silent = 1;
		dbcmd(dbproc, "declare @i int ");
		if (FAIL != dbsqlexec(dbproc))
			printf("IDLE ");
		silent = 0;
	}
	printf("\n");
}

static void
do_test(const char *query)
{
	int ret;

	printf("test query %s\n", query);
	dbcmd(dbproc, (char *) query);

	printf("sqlexec ");
	dbsqlexec(dbproc);

	check_state();

	printf("nextrow ");
	dbnextrow(dbproc);

	check_state();

	printf("nextrow ");
	dbnextrow(dbproc);

	check_state();

	printf("results ");
	dbresults(dbproc);

	check_state();

	printf("nextrow ");
	dbnextrow(dbproc);

	check_state();

	printf("nextrow ");
	dbnextrow(dbproc);

	check_state();

	check_idle = 0;
	for (;;) {
		printf("results \n");
		ret = dbresults(dbproc);
		check_state();
		if (ret != SUCCEED)
			break;

		while (dbnextrow(dbproc) == SUCCEED)
			;
	}
}

int main(int argc, char *argv[])
{
	LOGINREC      *login;        /* Our login information. */

	read_login_info(argc, argv);

	if (dbinit() == FAIL)
		exit(1);

	dberrhandle((EHANDLEFUNC)err_handler);
	dbmsghandle((MHANDLEFUNC)msg_handler);

	login = dblogin();
	DBSETLUSER(login, USER);
	DBSETLPWD(login, PASSWORD);
	DBSETLAPP(login, __FILE__);

	dbproc = dbopen(login, SERVER);
	dbloginfree(login);
	if (!dbproc)
		exit(1);

	query("create table #dummy (s char(10))");
	query("insert into #dummy values('xxx')");
	query("drop proc done_test");
	query("create proc done_test @a varchar(10) output as select * from #dummy");

	check_idle = 1;

	// normal row
	do_test("select * from #dummy");
	// normal row with no count
	query("set nocount on");
	do_test("select * from #dummy");
	query("set nocount off");
	// normal row without rows
	do_test("select * from #dummy where 0=1");
	// error query
	do_test("select dklghdlgkh from #dummy");
	// store procedure call with output parameters
	do_test("declare @s varchar(10) exec done_test @s output");

	do_test("declare @i int");

	query("drop proc done_test");

	dbexit();
	return 0;
}

static int err_handler(dbproc, severity, dberr, oserr, dberrstr, oserrstr)
DBPROCESS       *dbproc;
int             severity;
int             dberr;
int             oserr;
char            *dberrstr;
char            *oserrstr;
{
	if (silent)
		return INT_CANCEL;

	fprintf (stderr, "DB-Library error (severity %d):\n\t%s\n", severity, dberrstr);

	if (oserr != DBNOERR)
		fprintf (stderr, "Operating-system error:\n\t%s\n", oserrstr);

	return INT_CANCEL;
}

static int msg_handler(dbproc, msgno, msgstate, severity, msgtext, 
                srvname, procname, line)

DBPROCESS       *dbproc;
DBINT           msgno;
int             msgstate;
int             severity;
char            *msgtext;
char            *srvname;
char            *procname;
int     	line;

{
	if (silent)
		return 0;

	fprintf (stderr, "Msg %d, Level %d, State %d\n", 
	        (int) msgno, severity, msgstate);

	if (strlen(srvname) > 0)
		fprintf (stderr, "Server '%s', ", srvname);
	if (strlen(procname) > 0)
		fprintf (stderr, "Procedure '%s', ", procname);
	if (line > 0)
		fprintf (stderr, "Line %d", line);

	fprintf (stderr, "\n\t%s\n", msgtext);

	return(0);
}
