/* 
 * Purpose: Test data retrieval accuracy and cancelling results
 * Functions: dbbind dbcancel dbcmd dbcolname dbnextrow dbnumcols dbresults dbsqlexec 
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

static char software_version[] = "$Id: t0005.c,v 1.19 2006-01-30 15:31:56 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char **argv)
{
	const int rows_to_add = 50;
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	DBINT testint;
	char cmd[1024];
	int failed = 0;

	set_malloc_options();

	read_login_info(argc, argv);
	fprintf(stdout, "Start\n");
	add_bread_crumb();


	dbinit();

	add_bread_crumb();
	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	add_bread_crumb();
	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0005");
	DBSETLHOST(login, "ntbox.dntis.ro");

	fprintf(stdout, "About to open\n");

	add_bread_crumb();
	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	add_bread_crumb();
	dbloginfree(login);
	add_bread_crumb();

	add_bread_crumb();

	fprintf(stdout, "creating table\n");
	if (SUCCEED != dbcmd(dbproc, "create table #dblib0005 " "(i int not null, s char(10) not null)")) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}

	if (SUCCEED != dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}

	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "insert\n");
	for (i = 1; i < rows_to_add; i++) {
		sprintf(cmd, "insert into #dblib0005 values (%d, 'row %04d')", i, i);
		dbcmd(dbproc, cmd);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}


	sprintf(cmd, "select * from #dblib0005 where i < 5 order by i");
	fprintf(stdout, "%s\n", cmd);
	dbcmd(dbproc, cmd);
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
	dbbind(dbproc, 2, STRINGBIND, -1, (BYTE *) teststr);
	add_bread_crumb();

	add_bread_crumb();

	for (i = 1; i < 5; i++) {
	char expected[1024];

		sprintf(expected, "row %04d", i);

		add_bread_crumb();


		testint = -1;
		strcpy(teststr, "bogus");

		add_bread_crumb();
		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			exit(1);
		}
		add_bread_crumb();
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

	fprintf(stdout, "This query should succeeded as we have fetched exactly the.\n");
	fprintf(stdout, "number of rows in the result set\n");

	sprintf(cmd, "select * from #dblib0005 where i < 6 order by i");
	fprintf(stdout, "%s\n", cmd);
	if (SUCCEED != dbcmd(dbproc, cmd)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}
	if (SUCCEED != dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbsqlexec should have succeeded but didn't\n", __FILE__, __LINE__);
		failed = 1;
	}
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
	dbbind(dbproc, 2, STRINGBIND, -1, (BYTE *) teststr);
	add_bread_crumb();

	add_bread_crumb();

	for (i = 1; i < 5; i++) {
	char expected[1024];

		sprintf(expected, "row %04d", i);

		add_bread_crumb();


		testint = -1;
		strcpy(teststr, "bogus");

		add_bread_crumb();
		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			exit(1);
		}
		add_bread_crumb();
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

	fprintf(stdout, "This query should fail as we have not fetched all the\n");
	fprintf(stdout, "rows in the result set\n");

	sprintf(cmd, "select * from #dblib0005 where i > 950 order by i");
	fprintf(stdout, "%s\n", cmd);
	if (SUCCEED != dbcmd(dbproc, cmd)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}
	if (SUCCEED == dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbsqlexec should have failed but didn't\n", __FILE__, __LINE__);
		failed = 1;
	}


	fprintf(stdout, "calling dbcancel to flush results\n");
	dbcancel(dbproc);

	fprintf(stdout, "Dropping proc\n");
	add_bread_crumb();
	sprintf(cmd, "if object_id('t0005_proc') is not null drop procedure t0005_proc");
	dbcmd(dbproc, cmd);
	add_bread_crumb();
	dbsqlexec(dbproc);
	add_bread_crumb();
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	add_bread_crumb();

	fprintf(stdout, "creating proc\n");
	sprintf(cmd, "create proc t0005_proc (@b int out) as\nbegin\n"
		"select * from #dblib0005 where i < 6 order by i\n" "select @b = 42\n" "end\n");

	fprintf(stdout, "%s\n", cmd);

	dbcmd(dbproc, cmd);
	if (dbsqlexec(dbproc) == FAIL) {
		add_bread_crumb();
		fprintf(stderr, "%s:%d: failed to create procedure\n", __FILE__, __LINE__);
		failed = 1;
	}
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "calling proc\n");
	sprintf(cmd, "declare @myout int exec t0005_proc @b = @myout output");
	fprintf(stdout, "%s\n", cmd);

	dbcmd(dbproc, cmd);
	if (dbsqlexec(dbproc) == FAIL) {
		add_bread_crumb();
		fprintf(stderr, "%s:%d: failed to call procedure\n", __FILE__, __LINE__);
		failed = 1;
	}
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
	dbbind(dbproc, 2, STRINGBIND, -1, (BYTE *) teststr);
	add_bread_crumb();

	for (i = 1; i < 6; i++) {
	char expected[1024];

		sprintf(expected, "row %04d", i);

		add_bread_crumb();


		testint = -1;
		strcpy(teststr, "bogus");

		add_bread_crumb();
		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			exit(1);
		}
		add_bread_crumb();
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
	add_bread_crumb();

	fprintf(stdout, "This next command should succeed as we have fetched exactly the.\n");
	fprintf(stdout, "number of rows in the result set\n");

	sprintf(cmd, "select getdate()");
	fprintf(stdout, "%s\n", cmd);
	if (SUCCEED != dbcmd(dbproc, cmd)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		failed = 1;
	}
	if (SUCCEED != dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbsqlexec should have succeeded but didn't\n", __FILE__, __LINE__);
		failed = 1;
	}
	add_bread_crumb();

	dbcancel(dbproc);

	dbcmd(dbproc, "drop procedure t0005_proc");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	dbexit();
	add_bread_crumb();

	fprintf(stdout, "dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	free_bread_crumb();
	return failed ? 1 : 0;
}
