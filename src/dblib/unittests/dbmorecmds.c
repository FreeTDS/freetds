/* 
 * Purpose: Test behaviour of dbmorecmds()
 * Functions: dbmorecmds 
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <sqlfront.h>
#include <sqldb.h>

#include "common.h"

static char software_version[] = "$Id: dbmorecmds.c,v 1.10 2005-05-23 08:06:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version,	no_unused_var_warn };

int failed = 0;
const static char query[] = "select count(*) from sysusers\n"
			    "select name from sysobjects compute count(name)\n";
int
main(int argc, char **argv)
{
	const int rows_to_add = 10;
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i, nresults;

	set_malloc_options();

	read_login_info(argc, argv);
	fprintf(stdout, "Start\n");
	add_bread_crumb();

	/* Fortify_EnterScope(); */
	dbinit();

	add_bread_crumb();
	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	add_bread_crumb();

	fprintf(stdout, "after bread crumb\n");

	login = dblogin();
	fprintf(stdout, "after dblogin\n");
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0024");

	fprintf(stdout, "About to open [%s][%s]\n", PASSWORD, USER);

	add_bread_crumb();

	fprintf(stdout, "After second bread crumb\n");

	dbproc = dbopen(login, SERVER);
	fprintf(stdout, "After dbopen [%s]\n", SERVER);

	if (strlen(DATABASE)) {
		fprintf(stdout, "About to dbuse [%s]\n", DATABASE);
		dbuse(dbproc, DATABASE);
	}
	add_bread_crumb();
	dbloginfree(login);
	add_bread_crumb();

	fprintf(stdout, "After dbuse [%s]\n", DATABASE);
	add_bread_crumb();

	fprintf(stdout, "creating table\n");
	dbcmd(dbproc, "create table #dblib0024 (i int not null, s char(10) not null)");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "insert\n");
	for (i = 0; i < rows_to_add; i++) {
		char cmd[1024];

		sprintf(cmd, "insert into #dblib0024 values (%d, 'row %03d')", i, i);
		fprintf(stdout, "%s\n", cmd);
		dbcmd(dbproc, cmd);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}

	fprintf(stdout, "select 1\n");
	dbcmd(dbproc, "select count(*) from #dblib0024 -- order by i");
	dbsqlexec(dbproc);
	add_bread_crumb();

	nresults = 0;

	if (dbresults(dbproc) == SUCCEED) {
		do {
			while (dbnextrow(dbproc) != NO_MORE_ROWS);
			nresults++;
		} while (dbmorecmds(dbproc) == SUCCEED);
	}

	/* dbmorecmds should return success 0 times for select 1 */
	if (nresults != 1) {
		add_bread_crumb();
		failed = 1;
		fprintf(stdout, "Was expecting nresults == 1.\n");
		exit(1);
	}

	dbcancel(dbproc);

	fprintf(stdout, query);
	dbcmd(dbproc, query);
	dbsqlexec(dbproc);

	nresults = 0;

	do {
		if (dbresults(dbproc) == SUCCEED) {
			while (dbnextrow(dbproc) != NO_MORE_ROWS);
			nresults++;
		}
	} while (dbmorecmds(dbproc) == SUCCEED);


	/* dbmorecmds should return success 2 times for select 2 */
	if (nresults != 2) {	/* two results sets plus a return code */
		add_bread_crumb();
		failed = 1;
		fprintf(stdout, "nresults was %d; was expecting nresults = 2.\n", nresults);
		exit(1);
	}

	/* end of test processing */

	add_bread_crumb();
	dbexit();
	add_bread_crumb();

	fprintf(stdout, "dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	free_bread_crumb();
	return failed ? 1 : 0;
}
