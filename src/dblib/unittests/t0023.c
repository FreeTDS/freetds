/* 
 * Purpose: Test retrieving compute rows
 * Functions: dbaltbind dbaltcolid dbaltop dbalttype dbnumalts
 */

#include "common.h"
#include <assert.h>

static int got_error = 0;
static int compute_supported = 1;

static int
compute_msg_handler(DBPROCESS * dbproc TDS_UNUSED, DBINT msgno TDS_UNUSED, int state TDS_UNUSED, int severity TDS_UNUSED,
		    char *text TDS_UNUSED, char *server TDS_UNUSED, char *proc TDS_UNUSED, int line TDS_UNUSED)
{
	if (strstr(text, "compute"))
		compute_supported = 0;
	else
		got_error = 1;
	return 0;
}

TEST_MAIN()
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

	printf("Starting %s\n", argv[0]);

	/* Fortify_EnterScope(); */
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0023");

	printf("About to open\n");

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
		fprintf(stderr, "Unexpected error from query.\n");
		exit(1);
	}
	while ((i=dbresults(dbproc)) != NO_MORE_RESULTS) {
		while (dbnextrow(dbproc) != NO_MORE_ROWS)
			continue;
	}

	printf("creating table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	printf("insert\n");

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

	printf("select\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stderr, "Was expecting a result set.\n");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is %s\n", i, dbcolname(dbproc, i));

	printf("binding row columns\n");
	if (SUCCEED != dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & rowint)) {
		fprintf(stderr, "Had problem with bind col1\n");
		abort();
	}
	if (SUCCEED != dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) rowchar)) {
		fprintf(stderr, "Had problem with bind col2\n");
		abort();
	}
	if (SUCCEED != dbbind(dbproc, 3, STRINGBIND, 0, (BYTE *) rowdate)) {
		fprintf(stderr, "Had problem with bind col3\n");
		abort();
	}

	printf("testing compute clause 1\n");

	if (dbnumalts(dbproc, 1) != 1) {
		fprintf(stderr, "Had problem with dbnumalts 1\n");
		abort();
	}

	if (dbalttype(dbproc, 1, 1) != SYBINT4) {
		fprintf(stderr, "Had problem with dbalttype 1, 1\n");
		abort();
	}

	if (dbaltcolid(dbproc, 1, 1) != 1) {
		fprintf(stderr, "Had problem with dbaltcolid 1, 1\n");
		abort();
	}

	if (dbaltop(dbproc, 1, 1) != SYBAOPSUM) {
		fprintf(stderr, "Had problem with dbaltop 1, 1\n");
		abort();
	}

	if (SUCCEED != dbaltbind(dbproc, 1, 1, INTBIND, 0, (BYTE *) & computeint)) {
		fprintf(stderr, "Had problem with dbaltbind 1, 1\n");
		abort();
	}


	printf("testing compute clause 2\n");

	if (dbnumalts(dbproc, 2) != 1) {
		fprintf(stderr, "Had problem with dbnumalts 2\n");
		abort();
	}

	if (dbalttype(dbproc, 2, 1) != SYBDATETIME) {
		fprintf(stderr, "Had problem with dbalttype 2, 1\n");
		abort();
	}

	if (dbaltcolid(dbproc, 2, 1) != 3) {
		fprintf(stderr, "Had problem with dbaltcolid 2, 1\n");
		abort();
	}

	if (dbaltop(dbproc, 2, 1) != SYBAOPMAX) {
		fprintf(stderr, "Had problem with dbaltop 2, 1\n");
		abort();
	}

	if (SUCCEED != dbaltbind(dbproc, 2, 1, STRINGBIND, -1, (BYTE *) computedate)) {
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

	printf("%s %s\n", __FILE__, "OK");
	return 0;
}
