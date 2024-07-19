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

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	DBINT msgno;
	int num_rows, expected = 2;
	static const char table_name[] = "bcp_test";

	/* Variables for host file data to be copied to database. */
	char s1[11];
	char s2[11];
	char s3[11];

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
	DBSETLAPP(login, "bcp2.c unit test");
	BCP_SETL(login, TRUE);

	printf("About to open %s.%s\n", SERVER, DATABASE);

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	if (init(dbproc, table_name))
		doexit(1);

	/* set up and send the bcp */
	strcpy(s1, "test short");
	strcpy(s2, " ");
	strcpy(s3, "");

	if (bcp_init(dbproc, "bcp_test", NULL, "bcp.errors", DB_IN) != SUCCEED)
		doexit(1);

	bcp_bind(dbproc, (BYTE *)s1, 0, -1, (BYTE *)"", 1, SYBCHAR, 1);
	bcp_bind(dbproc, (BYTE *)s2, 0, -1, (BYTE *)"", 1, SYBCHAR, 2);
	bcp_bind(dbproc, (BYTE *)s3, 0, -1, (BYTE *)"", 1, SYBCHAR, 3);

	printf("Sending some rows... \n");
	if (bcp_sendrow(dbproc) == FAIL) {
		fprintf(stderr, "send failed\n");
		doexit(1);
	}

	strcpy(s2, "x");
	strcpy(s3, " ");
	if (bcp_sendrow(dbproc) == FAIL) {
		fprintf(stderr, "send failed\n");
		doexit(1);
	}

	/* In MSSQL the error is reported during bcp_done but all inserts are ignored */
	if (DBTDS_5_0 == DBTDS(dbproc)) {
		msgno = 20073;
		dbsetuserdata(dbproc, (BYTE*) &msgno);
		strcpy(s2, "");
		strcpy(s3, "");
		if (bcp_sendrow(dbproc) != FAIL) {
			fprintf(stderr, "send NULL succeeded\n");
			doexit(1);
		}
		dbsetuserdata(dbproc, NULL);

		/* end bcp. */
		if (bcp_done(dbproc) != expected) {
			fprintf(stderr, "Bulk copy unsuccessful.\n");
			doexit(1);
		}
	} else {
		/* end bcp. */
		if (bcp_done(dbproc) != expected) {
			fprintf(stderr, "Bulk copy unsuccessful.\n");
			doexit(1);
		}

		/* another insert with error */
		if (bcp_init(dbproc, "bcp_test", NULL, "bcp.errors", DB_IN) != SUCCEED)
			doexit(1);

		bcp_bind(dbproc, (BYTE *)s1, 0, -1, (BYTE *)"", 1, SYBCHAR, 1);
		bcp_bind(dbproc, (BYTE *)s2, 0, -1, (BYTE *)"", 1, SYBCHAR, 2);
		bcp_bind(dbproc, (BYTE *)s3, 0, -1, (BYTE *)"", 1, SYBCHAR, 3);

		strcpy(s2, "");
		strcpy(s3, "");
		if (bcp_sendrow(dbproc) == FAIL) {
			fprintf(stderr, "send failed\n");
			doexit(1);
		}

		/* end bcp. */
		msgno = 515;
		dbsetuserdata(dbproc, (BYTE*) &msgno);
		num_rows = bcp_done(dbproc);
		if (num_rows != -1) {
			fprintf(stderr, "Bulk copy successful. %d rows returned\n", num_rows);
			doexit(1);
		}
		dbsetuserdata(dbproc, NULL);
	}

	printf("done\n");

	/* check we inserted the expected number of rows row */
	num_rows = 0;
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		while (dbnextrow(dbproc) == REG_ROW)
			num_rows++;
	}
	if (num_rows != expected) {
		fprintf(stderr, "Expected %d row(s), got %d\n", expected, num_rows);
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
