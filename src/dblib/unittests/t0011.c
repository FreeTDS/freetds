/* 
 * Purpose: Test binding of string types
 * Functions: dbbind dbcmd dbcolname dbnextrow dbnumcols dbopen dbresults dbsqlexec 

 */

#include "common.h"

static int failed = 0;

static void insert_row(DBPROCESS * dbproc);
static int select_rows(DBPROCESS * dbproc, int bind_type);

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;

	read_login_info(argc, argv);
	printf("Starting %s\n", argv[0]);

	dbinit();

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0011");

	printf("About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	printf("Dropping table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	printf("creating table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	printf("insert\n");

	insert_row(dbproc);
	insert_row(dbproc);
	insert_row(dbproc);

	failed = select_rows(dbproc, STRINGBIND);

	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}

static int
select_rows(DBPROCESS * dbproc, int bind_type)
{
	char teststr[1024];
	char teststr2[1024];
	char testvstr[1024];
	DBINT testint;
	DBINT i;


	printf("select\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);


	if (dbresults(dbproc) != SUCCEED) {
		failed = 1;
		printf("Was expecting a result set.");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++) {
		printf("col %d is %s\n", i, dbcolname(dbproc, i));
	}

	if (SUCCEED != dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint)) {
		fprintf(stderr, "Had problem with bind\n");
		return 1;
	}
	if (SUCCEED != dbbind(dbproc, 2, bind_type, 0, (BYTE *) teststr)) {
		fprintf(stderr, "Had problem with bind\n");
		return 1;
	}
	if (SUCCEED != dbbind(dbproc, 3, bind_type, 0, (BYTE *) teststr2)) {
		fprintf(stderr, "Had problem with bind\n");
		return 1;
	}
	if (SUCCEED != dbbind(dbproc, 4, bind_type, 0, (BYTE *) testvstr)) {
		fprintf(stderr, "Had problem with bind\n");
		return 1;
	}

	i = 0;
	while (dbnextrow(dbproc) == REG_ROW) {
		i++;
		if (testint != i) {
			printf("Failed.  Expected i to be |%d|, was |%d|\n", testint, i);
			return 1;
		}
		printf("c:  %s$\n", teststr);
		printf("c2: %s$\n", teststr2);
		printf("vc: %s$\n", testvstr);
		if (bind_type == STRINGBIND) {
		} else {
		}
	}
	return 0;
}

static void
insert_row(DBPROCESS * dbproc)
{
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
}
