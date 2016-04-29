/* 
 * Purpose: Test retrieving compute rows
 * Functions: dbaltbind dbaltcolid dbaltop dbalttype dbnumalts
 */

#include "common.h"
#include <assert.h>

static int failed = 0;
static int got_error = 0;
static int compute_supported = 1;

static int
compute_msg_handler(DBPROCESS * dbproc, DBINT msgno, int state, int severity, char *text, char *server, char *proc, int line)
{
	if (strstr(text, "compute"))
		compute_supported = 0;
	else
		got_error = 1;
	return 0;
}

int
main(int argc, char *argv[])
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	DBINT rowint;
	DBCHAR rowchar[2];
	DBCHAR rowdate[32];

	DBINT rowtype;
	DBINT computeint;
	DBCHAR computedate[32];

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
	DBSETLAPP(login, "t0023");

	fprintf(stdout, "About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	sql_cmd(dbproc);
	dbmsghandle(compute_msg_handler);
	i = dbsqlexec(dbproc);
	dbmsghandle(syb_msg_handler);
	if (!compute_supported) {
		printf("compute clause not supported, skip test!\n");
		dbexit();
		return 0;
	}
	if (got_error) {
		failed = 1;
		fprintf(stderr, "Unexpected error from query.\n");
		exit(1);
	}
	while ((i=dbresults(dbproc)) != NO_MORE_RESULTS) {
		while (dbnextrow(dbproc) != NO_MORE_ROWS)
			continue;
	}

	fprintf(stdout, "creating table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	fprintf(stdout, "insert\n");

	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	fprintf(stdout, "select\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		failed = 1;
		fprintf(stdout, "Was expecting a result set.\n");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	fprintf(stdout, "binding row columns\n");
	if (SUCCEED != dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & rowint)) {
		failed = 1;
		fprintf(stderr, "Had problem with bind col1\n");
		abort();
	}
	if (SUCCEED != dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) rowchar)) {
		failed = 1;
		fprintf(stderr, "Had problem with bind col2\n");
		abort();
	}
	if (SUCCEED != dbbind(dbproc, 3, STRINGBIND, 0, (BYTE *) rowdate)) {
		failed = 1;
		fprintf(stderr, "Had problem with bind col3\n");
		abort();
	}

	fprintf(stdout, "testing compute clause 1\n");

	if (dbnumalts(dbproc, 1) != 1) {
		failed = 1;
		fprintf(stderr, "Had problem with dbnumalts 1\n");
		abort();
	}

	if (dbalttype(dbproc, 1, 1) != SYBINT4) {
		failed = 1;
		fprintf(stderr, "Had problem with dbalttype 1, 1\n");
		abort();
	}

	if (dbaltcolid(dbproc, 1, 1) != 1) {
		failed = 1;
		fprintf(stderr, "Had problem with dbaltcolid 1, 1\n");
		abort();
	}

	if (dbaltop(dbproc, 1, 1) != SYBAOPSUM) {
		failed = 1;
		fprintf(stderr, "Had problem with dbaltop 1, 1\n");
		abort();
	}

	if (SUCCEED != dbaltbind(dbproc, 1, 1, INTBIND, 0, (BYTE *) & computeint)) {
		failed = 1;
		fprintf(stderr, "Had problem with dbaltbind 1, 1\n");
		abort();
	}


	fprintf(stdout, "testing compute clause 2\n");

	if (dbnumalts(dbproc, 2) != 1) {
		failed = 1;
		fprintf(stderr, "Had problem with dbnumalts 2\n");
		abort();
	}

	if (dbalttype(dbproc, 2, 1) != SYBDATETIME) {
		failed = 1;
		fprintf(stderr, "Had problem with dbalttype 2, 1\n");
		abort();
	}

	if (dbaltcolid(dbproc, 2, 1) != 3) {
		failed = 1;
		fprintf(stderr, "Had problem with dbaltcolid 2, 1\n");
		abort();
	}

	if (dbaltop(dbproc, 2, 1) != SYBAOPMAX) {
		failed = 1;
		fprintf(stderr, "Had problem with dbaltop 2, 1\n");
		abort();
	}

	if (SUCCEED != dbaltbind(dbproc, 2, 1, STRINGBIND, -1, (BYTE *) computedate)) {
		failed = 1;
		fprintf(stderr, "Had problem with dbaltbind 2, 1\n");
		abort();
	}

	while ((rowtype = dbnextrow(dbproc)) != NO_MORE_ROWS) {

		if (rowtype == REG_ROW) {
			printf("gotten a regular row\n");
		}

		if (rowtype == 1) {
			printf("gotten a compute row for clause 1\n");
			printf("value of sum(col1) = %d\n", computeint);
		}

		if (rowtype == 2) {
			printf("gotten a compute row for clause 2\n");
			printf("value of max(col3) = %s\n", computedate);

		}
	}

	dbexit();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
