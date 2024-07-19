/* 
 * Purpose: Test behaviour of dbmorecmds()
 * Functions: dbmorecmds 
 */

#include "common.h"

TEST_MAIN()
{
	const int rows_to_add = 10;
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i, nresults;

	set_malloc_options();

	read_login_info(argc, argv);
	printf("Starting %s\n", argv[0]);

	/* Fortify_EnterScope(); */
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	printf("after dblogin\n");
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "dbmorecmds");

	printf("About to open [%s]\n", USER);

	dbproc = dbopen(login, SERVER);
	printf("After dbopen [%s]\n", SERVER);

	if (strlen(DATABASE)) {
		printf("About to dbuse [%s]\n", DATABASE);
		dbuse(dbproc, DATABASE);
	}
	dbloginfree(login);

	printf("After dbuse [%s]\n", DATABASE);

	printf("creating table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	printf("insert\n");
	for (i = 0; i < rows_to_add; i++) {
		sql_cmd(dbproc);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}

	printf("select one resultset\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	nresults = 0;

	if (dbresults(dbproc) == SUCCEED) {
		do {
			while (dbnextrow(dbproc) != NO_MORE_ROWS)
				continue;
			nresults++;
		} while (dbmorecmds(dbproc) == SUCCEED);
	}

	/* dbmorecmds should return success 0 times for select 1 */
	if (nresults != 1) {
		fprintf(stderr, "Was expecting nresults == 1.\n");
		exit(1);
	}

	dbcancel(dbproc);

	printf("select two resultsets\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	nresults = 0;

	do {
		if (dbresults(dbproc) == SUCCEED) {
			while (dbnextrow(dbproc) != NO_MORE_ROWS)
				continue;
			nresults++;
		}
	} while (dbmorecmds(dbproc) == SUCCEED);


	/* dbmorecmds should return success 2 times for select 2 */
	if (nresults != 2) {	/* two results sets plus a return code */
		fprintf(stderr, "nresults was %d; was expecting nresults = 2.\n", nresults);
		exit(1);
	}

	/* end of test processing */
	dbexit();

	printf("%s OK\n", __FILE__);
	return 0;
}
