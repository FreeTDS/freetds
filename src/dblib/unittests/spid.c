/*
 * Purpose: Retrieve the connection SPID.
 * Functions: dbspid
 */

#include "common.h"

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	DBINT expected_spid, actual_spid;
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
	DBSETLAPP(login, "spid");

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

#ifdef DBQUOTEDIDENT
	printf("QUOTED_IDENTIFIER is %s\n", (dbisopt(dbproc, DBQUOTEDIDENT, NULL))? "ON":"OFF");
#endif
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stderr, "error: expected a result set, none returned.\n");
		return 1;
	}

	if (SUCCEED != dbbind(dbproc, 1, INTBIND, -1, (BYTE *) & expected_spid)) {
		fprintf(stderr, "Had problem with bind\n");
		return 1;
	}
	if (REG_ROW != dbnextrow(dbproc)) {
		fprintf(stderr, "Failed.  Expected a row\n");
		return 1;
	}
	assert(expected_spid > 0);

	actual_spid = dbspid(dbproc);
	if (expected_spid != actual_spid) {
		fprintf(stderr, "Failed.  Expected spid to be %d, was %d\n", expected_spid, actual_spid);
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
