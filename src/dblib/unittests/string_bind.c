/*
 * Purpose: Test different string binding combinations.
 * Functions: dbbind
 */

#include "common.h"

static DBPROCESS *dbproc = NULL;
static int bind_len = -1;
static int expected_error = 0;
static const char *select_cmd = "select 'foo  '";

static void
test_row(int vartype, const char *vartype_name, const char *expected, int line)
{
	char str[11];
	size_t i;

	printf("%d: row type %s bind len %d\n", line, vartype_name, bind_len);

	if (dbcmd(dbproc, select_cmd) != SUCCEED) {
		fprintf(stderr, "error: dbcmd\n");
		exit(1);
	}
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stderr, "error: expected a result set, none returned.\n");
		exit(1);
	}

	memset(str, '$', sizeof(str));
	str[sizeof(str) - 1] = 0;
	if (dbbind(dbproc, 1, vartype, bind_len, (BYTE *) str) != SUCCEED) {
		fprintf(stderr, "Had problem with bind\n");
		exit(1);
	}
	if (dbnextrow(dbproc) != REG_ROW) {
		fprintf(stderr, "Failed.  Expected a row\n");
		exit(1);
	}

	assert(str[sizeof(str) - 1] == 0);
	if (vartype == CHARBIND) {
		/* not terminated space padded */
		char *p = strchr(str, '$');
		i = p ? p - str : sizeof(str);
	} else {
		/* terminated */
		char *p = strchr(str, 0);
		i = p - str + 1;
	}
	for (; i < sizeof(str)-1; ++i) {
		assert(str[i] == '$');
		str[i] = 0;
	}

	printf("str '%s'\n", str);
	if (strcmp(str, expected) != 0) {
		fprintf(stderr, "Expected '%s' string\n", expected);
		exit(1);
	}

	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		fprintf(stderr, "Was expecting no more rows\n");
		exit(1);
	}
	assert(expected_error == 0);
}

#define row(bind, expected) test_row(bind, #bind, expected, __LINE__)

TEST_MAIN()
{
	LOGINREC *login;

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
	DBSETLAPP(login, "string_bind");

	printf("About to open \"%s\"\n", SERVER);

	dbproc = dbopen(login, SERVER);
	if (!dbproc) {
		fprintf(stderr, "Unable to connect to %s\n", SERVER);
		return 1;
	}
	dbloginfree(login);

	dbsetuserdata(dbproc, (BYTE*) &expected_error);

	row(NTBSTRINGBIND, "foo");
	row(STRINGBIND, "foo  ");
	row(CHARBIND, "foo  ");

	bind_len = 4;
	row(NTBSTRINGBIND, "foo");
	expected_error = SYBECOFL;
	row(STRINGBIND, "foo");
	expected_error = SYBECOFL;
	row(CHARBIND, "foo ");

	bind_len = 5;
	row(NTBSTRINGBIND, "foo");
	expected_error = SYBECOFL;
	row(STRINGBIND, "foo ");
	row(CHARBIND, "foo  ");

	bind_len = 8;
	row(NTBSTRINGBIND, "foo");
	row(STRINGBIND, "foo    ");
	row(CHARBIND, "foo     ");

	bind_len = 3;
	expected_error = SYBECOFL;
	row(NTBSTRINGBIND, "fo");

	select_cmd = "select 123";

	bind_len = -1;
	row(NTBSTRINGBIND, "123");
	row(STRINGBIND, "123");
	row(CHARBIND, "123");

	bind_len = 4;
	row(NTBSTRINGBIND, "123");
	row(STRINGBIND, "123");
	row(CHARBIND, "123 ");

	bind_len = 6;
	row(NTBSTRINGBIND, "123");
	row(STRINGBIND, "123  ");
	row(CHARBIND, "123   ");

	bind_len = 3;
	expected_error = SYBECOFL;
	row(NTBSTRINGBIND, "12");
	expected_error = SYBECOFL;
	row(STRINGBIND, "12");
	row(CHARBIND, "123");

	bind_len = 2;
	expected_error = SYBECOFL;
	row(CHARBIND, "12");

	dbclose(dbproc);

	dbexit();
	return 0;
}
