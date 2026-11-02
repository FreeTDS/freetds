/*
 * Purpose: Test that a string truncated inside a multibyte sequence is reported
 * correctly, reporting the error used to crash, see GitHub issue #227.
 * The server truncates a surrogate pair storing it in a NVARCHAR(1) column.
 */

#include "common.h"

static int conversion_errors = 0;
static int wrong_errors = 0;

static int
err_handler(DBPROCESS *dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
	switch (dberr) {
	case SYBEICONVPARTIAL:
	case SYBEICONVI:
		printf("got expected error %d: %s\n", dberr, dberrstr);
		++conversion_errors;
		return INT_CANCEL;
	case SYBEICONVAVAIL:
		fprintf(stderr, "conversion wrongly reported as not available: %s\n", dberrstr);
		++wrong_errors;
		return INT_CANCEL;
	default:
		break;
	}
	return syb_err_handler(dbproc, severity, dberr, oserr, dberrstr, oserrstr);
}

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	DBINT value = 0;

	set_malloc_options();

	read_login_info(argc, argv);

	dbinit();

	dberrhandle(err_handler);
	dbmsghandle(syb_msg_handler);

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "iconv_partial");

	dbproc = dbopen(login, SERVER);
	if (!dbproc) {
		fprintf(stderr, "Unable to connect to %s\n", SERVER);
		return 1;
	}
	dbloginfree(login);

	/* requires an UTF-16 column from Microsoft server */
	if (dbtds(dbproc) < DBTDS_7_0) {
		dbclose(dbproc);
		dbexit();
		return 77;
	}

	dbcmd(dbproc, "DECLARE @test NVARCHAR(1) SET @test = 0x3CD884DF SELECT @test AS test, 1234 AS n");
	if (dbsqlexec(dbproc) != SUCCEED) {
		fprintf(stderr, "error: executing query\n");
		return 1;
	}

	/* DECLARE can produce a result without columns, skip it */
	do {
		if (dbresults(dbproc) != SUCCEED) {
			fprintf(stderr, "error: expected a result set\n");
			return 1;
		}
	} while (dbnumcols(dbproc) == 0);

	if (dbbind(dbproc, 2, INTBIND, -1, (BYTE *) & value) != SUCCEED) {
		fprintf(stderr, "error: binding column\n");
		return 1;
	}

	if (dbnextrow(dbproc) != REG_ROW) {
		fprintf(stderr, "error: expected a row\n");
		return 1;
	}

	/* the conversion problem should not affect the following column */
	if (value != 1234) {
		fprintf(stderr, "error: unexpected value %d\n", (int) value);
		return 1;
	}

	dbcancel(dbproc);
	dbclose(dbproc);
	dbexit();

	if (wrong_errors || !conversion_errors) {
		fprintf(stderr, "error: wrong conversion errors %d, right conversion errors %d\n", wrong_errors, conversion_errors);
		return 1;
	}

	printf("OK\n");
	return 0;
}
