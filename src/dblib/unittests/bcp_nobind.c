/*
 * Purpose: Test bcp functions, specifically some NULL behaviour
 * Functions: bcp_bind bcp_done bcp_init bcp_sendrow
 */

#include "common.h"

#include <assert.h>

static void
doexit(int value)
{
	dbexit();            /* always call dbexit before returning to OS */
	exit(value);
}

static int
init(DBPROCESS * dbproc, const char *name)
{
	RETCODE rc;

	printf("Dropping %s.%s..%s\n", SERVER, DATABASE, name);
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while ((rc=dbresults(dbproc)) == SUCCEED)
		continue;
	if (rc != NO_MORE_RESULTS)
		return 1;

	printf("Creating %s.%s..%s\n", SERVER, DATABASE, name);
	sql_cmd(dbproc);

	if (dbsqlexec(dbproc) == FAIL)
		return 1;
	while ((rc=dbresults(dbproc)) == SUCCEED)
		continue;
	if (rc != NO_MORE_RESULTS)
		return 1;
	printf("ok\n");
	return 0;
}

static int do_send(DBPROCESS* dbproc, DBINT expected_error)
{
	DBINT msgno;
	if (expected_error)
	{
		msgno = expected_error;
		dbsetuserdata(dbproc, (BYTE*)&msgno);
	}

	printf("Sending row... \n");
	int result = bcp_sendrow(dbproc);

	if (expected_error)
		dbsetuserdata(dbproc, NULL);

	if ( expected_error && result != FAIL ) {
		fprintf(stderr, "send succeeded when we expected failure\n");
		doexit(1);
	}

	else if (!expected_error && result == FAIL) {
		fprintf(stderr, "send failed\n");
		doexit(1);
	}
	return !expected_error;
}

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int num_rows, expected =0, actual;
	static const char table_name[] = "bcp_nobind";
	bool is_ms;

	/* Variables for host file data to be copied to database. */
	int i3;

	set_malloc_options();

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	dbsetversion(DBVERSION_100);
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "bcp_nobind.c unit test");
	BCP_SETL(login, TRUE);

	printf("About to open %s.%s\n", SERVER, DATABASE);

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	if (init(dbproc, table_name))
		doexit(1);

	/* set up and send the bcp */
//		i1 int null,
//		i2 int null default(7),
//		i3 int not null default(9)

	if (bcp_init(dbproc, "bcp_nobind", NULL, "bcp.errors", DB_IN) != SUCCEED)
		doexit(1);

	/* Omit value for i1 and i2 (nullable int with/without defaults) - Default should apply */
	i3 = 13;
	bcp_bind(dbproc, (BYTE *)&i3, 0, -1, NULL, 0, SYBINT4, 3);

	expected += do_send(dbproc, FALSE);
	expected += do_send(dbproc, FALSE);

	actual = bcp_done(dbproc); 
	if (actual != expected) {
		fprintf(stderr, "Bulk copy unsuccessful (expected %d rows, got %d)\n", expected, actual);
		doexit(1);
	}

	/* Test write a null value to non-nullable field with default value.
	 *
	 * We have to start a new batch because if this fails (as expected),
	 * it leaves the stream in an unrecoverable state. bcp_send_record()
	 * actually writes the row up to the point it discovers it can't encode
	 * a null value.
	 * NOTE: this undoes any previous bindings, which suits us as we want to test
	 * column 3 being unbound now. */
	if (bcp_init(dbproc, "bcp_nobind", NULL, "bcp.errors", DB_IN) != SUCCEED)
		doexit(1);

	/* BCP protocol cannot signal to use default value for a non-nullable field
	 * (because non-nullable fields are packed without any length or other
	 * metadata that could encode NULL).
	 * However ASE gives the client enough information to fill in the default
	 * value on the client side, so ASE can pass this test. MSSQL can't.
	 */
	is_ms = DBTDS_5_0 != DBTDS(dbproc);
	do_send(dbproc, is_ms ? 20073 : 0);

	if (is_ms)
	{
		/* Clear the error state of this partial send */
		DBINT msgid = 4002;
		dbsetuserdata(dbproc, (BYTE*)&msgid);
		bcp_done(dbproc);
		dbsetuserdata(dbproc, NULL);
	}
	else
	{
		bcp_done(dbproc);
		++expected;
	}

	printf("done\n");

	/* check we inserted the expected number of rows */
	num_rows = 0;
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		while (dbnextrow(dbproc) == REG_ROW)
			num_rows++;
	}
	fprintf(stderr, "Expected %d row(s), got %d\n", expected, num_rows);
	if (num_rows != expected) {
		doexit(1);
	}

	printf("Dropping table %s\n", table_name);
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	dbexit();

	printf("%s OK\n", __FILE__);
	return 0;
}
