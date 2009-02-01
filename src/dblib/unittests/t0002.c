/* 
 * Purpose: Test buffering
 * Functions: dbclrbuf dbgetrow dbsetopt 
 */
#if 0
	# Find functions with:
	sed -ne'/db/ s/.*\(db[[:alnum:]_]*\)(.*/\1/gp' src/dblib/unittests/t0002.c |sort -u |fmt
#endif

#include "common.h"
#include <assert.h>

static char software_version[] = "$Id: t0002.c,v 1.23 2009-02-01 22:29:39 jklowden Exp $";
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
	int i, iresults;
	char teststr[1024];

	const int buffer_count = 10;
	const int rows_to_add = 50;

	set_malloc_options();

	read_login_info(argc, argv);

	fprintf(stdout, "Starting %s\n", argv[0]);
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
#ifdef MICROSOFT_DBLIB
	dbsetopt(dbproc, DBBUFFER, "10");
#else
	dbsetopt(dbproc, DBBUFFER, "10", 0);
#endif
	add_bread_crumb();

	add_bread_crumb();
	sql_cmd(dbproc,  INPUT); /* drop table if exists */
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

	sql_cmd(dbproc,  INPUT); /* create table */
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	for (i = 1; i <= rows_to_add; i++) {
		sql_cmd(dbproc, INPUT);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}

	sql_cmd(dbproc, INPUT);	/* two result sets */
	dbsqlexec(dbproc);
	add_bread_crumb();

	for (iresults=1; iresults <= 2; iresults++ ) {
		fprintf(stdout, "fetching resultset %i\n", iresults);
		if (dbresults(dbproc) != SUCCEED) {
			add_bread_crumb();
			fprintf(stderr, "Was expecting a result set %d.\n", iresults);
			if( iresults == 2 )
				fprintf(stderr, "Buffering with multiple resultsets is broken.\n");
			exit(1);
		}
		add_bread_crumb();

		for (i = 1; i <= dbnumcols(dbproc); i++) {
			add_bread_crumb();
			printf("col %d is [%s]\n", i, dbcolname(dbproc, i));
			add_bread_crumb();
		}

		add_bread_crumb();
		dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
		add_bread_crumb();
		dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);
		add_bread_crumb();

		/* Fetch a result set */
		/* Second resultset stops at row 40 */
		for (i=0; i < rows_to_add - (iresults == 2 ? buffer_count : 0);) {

			fprintf(stdout, "clearing %d rows from buffer\n", buffer_count);
			dbclrbuf(dbproc, buffer_count);

			do {
				int rc;

				i++;
				add_bread_crumb();
				if (REG_ROW != (rc = dbnextrow(dbproc))) {
					failed = 1;
					fprintf(stderr, "Failed: Expected a row (%s:%d)\n", __FILE__, __LINE__);
					if (rc == BUF_FULL)
						fprintf(stderr, "Failed: dbnextrow returned BUF_FULL (%d).  Fix dbclrbuf.\n", rc);
					exit(1);
				}
				add_bread_crumb();
				verify(i, testint, teststr);
			} while (i % buffer_count);

			if (iresults == 1 && i == rows_to_add) {
				/* The buffer should be full */
				assert(BUF_FULL == dbnextrow(dbproc));
			}
				
		}
		if (iresults == 1) {
			fprintf(stdout, "clearing %d rows from buffer\n", buffer_count);
			dbclrbuf(dbproc, buffer_count);
			while (dbnextrow(dbproc) != NO_MORE_ROWS) {
				abort(); /* All rows were read: should not enter loop */
			}
		}
	}

	/* 
	 * Now test the buffered rows.  
	 * Should be operating on rows 31-40 of 2nd resultset 
	 */
	rc = dbgetrow(dbproc, 1);
	add_bread_crumb();
	if(rc != NO_MORE_ROWS)	/* row 1 is not among the 31-40 in the buffer */
		fprintf(stderr, "Failed: dbgetrow returned %d.\n", rc);
	assert(rc == NO_MORE_ROWS);

	rc = dbgetrow(dbproc, 31);
	add_bread_crumb();
	if(rc != REG_ROW)
		fprintf(stderr, "Failed: dbgetrow returned %d.\n", rc);
	assert(rc == REG_ROW);
	verify(31, testint, teststr);	/* first buffered row should be 31 */

	rc = dbnextrow(dbproc);
	add_bread_crumb();
	if(rc != REG_ROW)
		fprintf(stderr, "Failed: dbgetrow returned %d.\n", rc);
	assert(rc == REG_ROW);
	verify(32, testint, teststr);	/* next buffered row should be 32 */

	rc = dbgetrow(dbproc, 11);
	add_bread_crumb();
	assert(rc == NO_MORE_ROWS);	/* only 10 (not 11) rows buffered */

	rc = dbgetrow(dbproc, 40);
	add_bread_crumb();
	assert(rc == REG_ROW);
	verify(40, testint, teststr);	/* last buffered row should be 40 */

	/* Attempt dbnextrow when buffer has no space (10 out of 10 in use). */
	rc = dbnextrow(dbproc);
	assert(rc == BUF_FULL);

	dbclrbuf(dbproc, 3);		/* remove rows 31, 32, and 33 */

	rc = dbnextrow(dbproc);
	add_bread_crumb();
	assert(rc == REG_ROW);
	verify(41, testint, teststr);	/* fetch row from database, should be 41 */

	rc = dbnextrow(dbproc);
	add_bread_crumb();
	assert(rc == REG_ROW);
	verify(42, testint, teststr);	/* fetch row from database, should be 42 */

	/* buffer contains 9 rows (34-42) try removing 10 rows */
	dbclrbuf(dbproc, buffer_count);

	while (dbnextrow(dbproc) != NO_MORE_ROWS) {
		/* waste rows 43-50 */
	}

	dbclose(dbproc); /* close while buffer not cleared: OK */

	add_bread_crumb();
	dbexit();
	add_bread_crumb();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	free_bread_crumb();
	return failed ? 1 : 0;
}
