/*
 * Purpose: Check dbcanquery throw away rows and we can continue after the call
 * Functions: dbcanquery
 */

#include "common.h"

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	DBINT data;
	RETCODE erc;

	set_malloc_options();

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon as \"%s\"\n", USER);

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "canquery");

	printf("About to open \"%s\"\n", SERVER);

	dbproc = dbopen(login, SERVER);
	if (!dbproc) {
		fprintf(stderr, "Unable to connect to %s\n", SERVER);
		return 1;
	}
	dbloginfree(login);

	printf("Using database \"%s\"\n", DATABASE);
	if (strlen(DATABASE)) {
		erc = dbuse(dbproc, DATABASE);
		assert(erc == SUCCEED);
	}

	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stderr, "error: expected a result set, none returned.\n");
		return 1;
	}

	if (dbcanquery(dbproc) != SUCCEED) {
		fprintf(stderr, "error: unexpected error from dbcanquery.\n");
		return 1;
	}

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stderr, "error: expected a result set, none returned.\n");
		return 1;
	}

	if (SUCCEED != dbbind(dbproc, 1, INTBIND, -1, (BYTE *) & data)) {
		fprintf(stderr, "Had problem with bind\n");
		return 1;
	}

	if (REG_ROW != dbnextrow(dbproc)) {
		fprintf(stderr, "Failed.  Expected a row\n");
		return 1;
	}

	if (data != 2) {
		fprintf(stderr, "Failed. Expected row data to be 2, was %d\n", data);
		return 1;
	}

	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		fprintf(stderr, "Was expecting no more rows\n");
		return 1;
	}

	dbclose(dbproc);

	dbexit();
	return 0;
}
