/* 
 * Purpose: Test buffering
 * Functions: dbclrbuf dbgetrow dbsetopt 
 */
#if 0
	# Find functions with:
	sed -ne'/db/ s/.*\(db[[:alnum:]_]*\)(.*/\1/gp' src/dblib/unittests/t0002.c |sort -u |fmt
#endif

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <assert.h>
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

static char software_version[] = "$Id: t0002.c,v 1.11 2005-04-13 17:46:29 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int failed = 0;

static void
verify(int i, int testint, char *teststr)
{
	char expected[1024];

	sprintf(expected, "row %03d", i);

	if (testint != i) {
		failed = 1;
		fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i, testint);
		abort();
	}
	if (0 != strncmp(teststr, expected, strlen(expected))) {
		failed = 1;
		fprintf(stderr, "Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
		abort();
	}
	printf("Read a row of data -> %d %s\n", testint, teststr);
}

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	DBINT testint;
	STATUS rc;
	int i;
	char teststr[1024];

	const int rows_to_add = 50;
	const char tablename[] = "#dblib0002";
	const char drop_if_exists[] = "if exists ( select 1 "
						  "from tempdb..sysobjects "
						  "where id = object_id('tempdb..%s') )\n"
				      "\tdrop table %s\n";

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
	DBSETLAPP(login, "t0002");

	fprintf(stdout, "About to open %s..%s\n", SERVER, DATABASE);

	add_bread_crumb();
	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	add_bread_crumb();
	dbloginfree(login);
	add_bread_crumb();

	fprintf(stdout, "Setting row buffer to 10 rows\n");
	dbsetopt(dbproc, DBBUFFER, "10", 0);
	add_bread_crumb();

	add_bread_crumb();
	fprintf(stdout, drop_if_exists, tablename, tablename);
	dbfcmd(dbproc,  drop_if_exists, tablename, tablename);
	add_bread_crumb();
	dbsqlexec(dbproc);
	add_bread_crumb();
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	if (dbresults(dbproc) != NO_MORE_RESULTS) {
		fprintf(stdout, "Failed: dbresults call after NO_MORE_RESULTS should return NO_MORE_RESULTS.\n");
		failed = 1;
	}
	add_bread_crumb();

	fprintf(stdout, "create table %s (i int not null, s char(10) not null)\n", tablename);
	dbfcmd(dbproc,  "create table %s (i int not null, s char(10) not null)", tablename);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "insert into %s [10 rows]\n", tablename);
	for (i = 1; i <= rows_to_add; i++) {
	char cmd[1024];

		sprintf(cmd, "insert into %s values (%d, 'row %03d')", tablename, i, i);
		dbcmd(dbproc, cmd);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}

	fprintf(stdout, "select * from %s order by i\n", tablename);
	dbfcmd(dbproc,  "select * from %s order by i", tablename);
	dbsqlexec(dbproc);
	add_bread_crumb();


	if (dbresults(dbproc) != SUCCEED) {
		add_bread_crumb();
		fprintf(stderr, "Was expecting a result set.");
		exit(1);
	}
	add_bread_crumb();

	for (i = 1; i <= dbnumcols(dbproc); i++) {
		add_bread_crumb();
		printf("col %d is [%s]\n", i, dbcolname(dbproc, i));
		add_bread_crumb();
	}

	add_bread_crumb();
	dbbind(dbproc, 1, INTBIND, -1, (BYTE *) & testint);
	add_bread_crumb();
	dbbind(dbproc, 2, STRINGBIND, -1, (BYTE *) teststr);
	add_bread_crumb();

	for (i = 1; i <= 10; i++) {
		add_bread_crumb();
		if (REG_ROW != dbnextrow(dbproc)) {
			failed = 1;
			fprintf(stderr, "Failed.  Expected a row\n");
			exit(1);
		}
		add_bread_crumb();
		verify(i, testint, teststr);
	}

	rc = dbgetrow(dbproc, 1);
	add_bread_crumb();
	assert(rc == REG_ROW);
	verify(1, testint, teststr);

	rc = dbnextrow(dbproc);
	add_bread_crumb();
	assert(rc == REG_ROW);
	verify(2, testint, teststr);

	rc = dbgetrow(dbproc, 11);
	add_bread_crumb();
	assert(rc == NO_MORE_ROWS);

	rc = dbgetrow(dbproc, 10);
	add_bread_crumb();
	assert(rc == REG_ROW);
	verify(10, testint, teststr);

	rc = dbnextrow(dbproc);
	assert(rc == BUF_FULL);

	dbclrbuf(dbproc, 3);

	rc = dbnextrow(dbproc);
	add_bread_crumb();
	assert(rc == REG_ROW);
	verify(11, testint, teststr);

	rc = dbnextrow(dbproc);
	add_bread_crumb();
	assert(rc == REG_ROW);
	verify(12, testint, teststr);


	add_bread_crumb();
	dbexit();
	add_bread_crumb();


	fprintf(stdout, "dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	free_bread_crumb();
	return failed ? 1 : 0;
}
