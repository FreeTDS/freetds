/* 
 * Purpose: Test buffering.  As of April 2005, jkl believes this test is broken. 
 * Functions: dbclrbuf dbsetopt 
 */

#include "common.h"

int
main(int argc, char **argv)
{
	const int rows_to_add = 48;
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	DBINT testint;
	DBINT last_read = -1;

	set_malloc_options();

	read_login_info(argc, argv);

	fprintf(stdout, "Starting %s\n", argv[0]);

	/* Fortify_EnterScope(); */
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0008");
	DBSETLHOST(login, "ntbox.dntis.ro");

	fprintf(stdout, "About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

#ifdef MICROSOFT_DBLIB
	dbsetopt(dbproc, DBBUFFER, "25");
#else
	dbsetopt(dbproc, DBBUFFER, "25", 0);
#endif

	fprintf(stdout, "creating table\n");
	dbcmd(dbproc, "create table #dblib0008 (i int not null, s char(10) not null)");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "insert\n");
	for (i = 1; i <= rows_to_add; i++) {
	char cmd[1024];

		sprintf(cmd, "insert into #dblib0008 values (%d, 'row %03d')", i, i);
		dbcmd(dbproc, cmd);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}

	fprintf(stdout, "select\n");
	dbcmd(dbproc, "select * from #dblib0008 order by i");
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stdout, "Was expecting a result set.");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
	dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);

	for (i = 1; i <= rows_to_add; i++) {
	char expected[1024];

		sprintf(expected, "row %03d", i);

		if (i % 25 == 0) {
			dbclrbuf(dbproc, 25);
		}

		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "dblib failed for %s, dbnextrow1\n", __FILE__);
			dbexit();
			return 1;
		}
		last_read = testint;
		if (testint < 1 || testint > rows_to_add) {
			fprintf(stderr, "dblib failed for %s testint = %d\n", __FILE__, (int) testint);
			exit(1);
		}
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

	if (REG_ROW == dbnextrow(dbproc)) {
		fprintf(stderr, "dblib failed for %s, dbnextrow2\n", __FILE__);
		dbexit();
		return 1;
	}

	dbexit();

	fprintf(stdout, "%s %s (last_read: %d)\n", __FILE__, ((last_read != rows_to_add)? "failed!" : "OK"), (int) last_read);
	return (last_read == rows_to_add) ? 0 : 1;
}
