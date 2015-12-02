/*
 * Purpose: Retrieve the connection SPID.
 * Functions: dbspid
 */

#include "common.h"

int failed = 0;


int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	DBINT expected_spid, actual_spid;
    RETCODE erc;

	set_malloc_options();

	read_login_info(argc, argv);
	if (argc > 1) {
		argc -= optind;
		argv += optind;
	}

	fprintf(stdout, "Starting %s\n", argv[0]);

	/* Fortify_EnterScope(); */
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon as \"%s\"\n", USER);

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0001");

	if (argc > 1) {
        int i;
		printf("server and login timeout overrides (%s and %s) detected\n", argv[0], argv[1]);
		strcpy(SERVER, argv[0]);
		i = atoi(argv[1]);
		if (i) {
			i = dbsetlogintime(i);
			printf("dbsetlogintime returned %s.\n", (i == SUCCEED)? "SUCCEED" : "FAIL");
		}
	}

	fprintf(stdout, "About to open \"%s\"\n", SERVER);

	dbproc = dbopen(login, SERVER);
	if (!dbproc) {
		fprintf(stderr, "Unable to connect to %s\n", SERVER);
		return 1;
	}
	dbloginfree(login);

	fprintf(stdout, "Using database \"%s\"\n", DATABASE);
	if (strlen(DATABASE)) {
		erc = dbuse(dbproc, DATABASE);
		assert(erc == SUCCEED);
	}

#ifdef DBQUOTEDIDENT
	fprintf(stdout, "QUOTED_IDENTIFIER is %s\n", (dbisopt(dbproc, DBQUOTEDIDENT, NULL))? "ON":"OFF");
#endif
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		failed = 1;
		fprintf(stderr, "error: expected a result set, none returned.\n");
		exit(1);
	}

	if (SUCCEED != dbbind(dbproc, 1, INTBIND, -1, (BYTE *) & expected_spid)) {
		failed = 1;
		fprintf(stderr, "Had problem with bind\n");
		abort();
	}
    if (REG_ROW != dbnextrow(dbproc)) {
		failed = 1;
		fprintf(stderr, "Failed.  Expected a row\n");
		exit(1);
	}
    assert(expected_spid > 0);

    actual_spid = dbspid(dbproc);
	if (expected_spid != actual_spid) {
		failed = 1;
		fprintf(stderr, "Failed.  Expected spid to be %d, was %d\n", expected_spid, actual_spid);
		abort();
	}

	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		failed = 1;
		fprintf(stderr, "Was expecting no more rows\n");
		exit(1);
	}

    dbclose(dbproc);

    actual_spid = dbspid(dbproc);
    if (-1 != actual_spid) {
        failed = 1;
		fprintf(stderr, "Failed.  Expected spid to be -1, was %d\n", actual_spid);
		abort();
    }

	dbexit();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
