/*
 * Purpose: Test that using a not available client character set is reported
 * once and with the name of the character set.
 */

#include "common.h"

#define CHARSET "NOSUCHCHARSET"

static int charset_errors = 0;
static int wrong_errors = 0;

static int
err_handler(DBPROCESS *dbproc TDS_UNUSED, int severity TDS_UNUSED, int dberr, int oserr TDS_UNUSED, char *dberrstr,
	    char *oserrstr TDS_UNUSED)
{
	if (dberr != SYBEICONVAVAIL) {
		fprintf(stderr, "unexpected error %d: %s\n", dberr, dberrstr);
		++wrong_errors;
		return INT_CANCEL;
	}

	printf("got expected error: %s\n", dberrstr);
	++charset_errors;
	if (strstr(dberrstr, "'" CHARSET "'") == NULL) {
		fprintf(stderr, "character set name not reported: %s\n", dberrstr);
		++wrong_errors;
	}
	return INT_CANCEL;
}

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;

	set_malloc_options();

	read_login_info(argc, argv);

	dbinit();

	dberrhandle(err_handler);
	dbmsghandle(syb_msg_handler);

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "charset_error");
	DBSETLCHARSET(login, CHARSET);

	dbproc = dbopen(login, SERVER);
	dbloginfree(login);

	if (dbproc) {
		fprintf(stderr, "error: connection should fail\n");
		dbclose(dbproc);
		return 1;
	}

	dbexit();

	if (wrong_errors || charset_errors != 1) {
		fprintf(stderr, "error: wrong errors %d, character set errors %d\n", wrong_errors, charset_errors);
		return 1;
	}

	printf("OK\n");
	return 0;
}
