/*
 * Purpose: Test we can send another query after beginning transaction
 * Some server some additional tokens which can prevent second query to work
 * correctly.
 */

#include "common.h"

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	DBINT testint = -1;

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "pending");

	printf("About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);


	/* first query, start transactions */
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	/* second query, select */
	printf("second select\n");

	if (SUCCEED != sql_cmd(dbproc) || SUCCEED != dbsqlexec(dbproc)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		exit(1);
	}

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stderr, "Was expecting a result set.");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);

	if (REG_ROW != dbnextrow(dbproc)) {
		fprintf(stderr, "Failed.  Expected a row\n");
		exit(1);
	}
	if (testint != 634) {
		fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i, (int) testint);
		exit(1);
	}
	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		fprintf(stderr, "No other rows expected\n");
	}

	/* third query, commit */
	if (SUCCEED != sql_cmd(dbproc)) {
		fprintf(stderr, "%s:%d: dbcmd failed\n", __FILE__, __LINE__);
		exit(1);
	}

	while (dbresults(dbproc) != NO_MORE_RESULTS)
		continue;

	dbexit();

	printf("ok\n");
	return 0;
}
