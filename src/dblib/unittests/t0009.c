/* ================================= t0009.c =================================
 * 
 *  Def:   Test to see if dbbind can handle a varlen of 0 with a 
 *         column bound as STRINGBIND and a database column of CHAR.
 * 
 * ===========================================================================
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



static char software_version[] = "$Id: t0009.c,v 1.11 2004-09-09 08:54:49 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };



int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	DBINT testint;

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
	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0009");
	DBSETLHOST(login, "ntbox.dntis.ro");

	fprintf(stdout, "About to open\n");

	add_bread_crumb();
	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	add_bread_crumb();
	dbloginfree(login);
	add_bread_crumb();

#ifdef MICROSOFT_DBLIB
	dbsetopt(dbproc, DBBUFFER, "100");
#else
	dbsetopt(dbproc, DBBUFFER, "100", 0);
#endif
	add_bread_crumb();

	fprintf(stdout, "Dropping table\n");
	add_bread_crumb();
	dbcmd(dbproc, "drop table #dblib0009");
	add_bread_crumb();
	dbsqlexec(dbproc);
	add_bread_crumb();
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	add_bread_crumb();

	fprintf(stdout, "creating table\n");
	dbcmd(dbproc, "create table #dblib0009 (i int not null, s char(10) not null)");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "insert\n");
	dbcmd(dbproc, "insert into #dblib0009 values (1, 'abcdef')");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	dbcmd(dbproc, "insert into #dblib0009 values (2, 'abc')");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}


	fprintf(stdout, "select\n");
	dbcmd(dbproc, "select * from #dblib0009 order by i");
	dbsqlexec(dbproc);
	add_bread_crumb();


	if (dbresults(dbproc) != SUCCEED) {
		add_bread_crumb();
		fprintf(stdout, "Was expecting a result set.");
		exit(1);
	}
	add_bread_crumb();

	for (i = 1; i <= dbnumcols(dbproc); i++) {
		add_bread_crumb();
		printf("col %d is %s\n", i, dbcolname(dbproc, i));
		add_bread_crumb();
	}

	add_bread_crumb();
	dbbind(dbproc, 1, INTBIND, -1, (BYTE *) & testint);
	add_bread_crumb();
	dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);
	add_bread_crumb();

	add_bread_crumb();


	if (REG_ROW != dbnextrow(dbproc)) {
		fprintf(stderr, "dblib failed for %s\n", __FILE__);
		exit(1);
	}
	if (0 != strcmp("abcdef    ", teststr)) {
		fprintf(stderr, "Expected |%s|, found |%s|\n", "abcdef", teststr);
		fprintf(stderr, "dblib failed for %s\n", __FILE__);
		exit(1);
	}

	if (REG_ROW != dbnextrow(dbproc)) {
		fprintf(stderr, "dblib failed for %s\n", __FILE__);
		exit(1);
	}
	if (0 != strcmp("abc       ", teststr)) {
		fprintf(stderr, "Expected |%s|, found |%s|\n", "abc", teststr);
		fprintf(stderr, "dblib failed for %s\n", __FILE__);
		exit(1);
	}

	add_bread_crumb();
	dbexit();
	add_bread_crumb();

	fprintf(stderr, "dblib passed for %s\n", __FILE__);
	free_bread_crumb();
	return 0;
}
