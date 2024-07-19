/* 
 * Purpose: Test to see if row buffering and blobs works correctly.
 * Functions: dbbind dbnextrow dbopen dbresults dbsqlexec dbgetrow
 */

#include "common.h"

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	DBINT testint;

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
	DBSETLAPP(login, "text_buffer");
	DBSETLHOST(login, "ntbox.dntis.ro");

	printf("About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

#ifdef MICROSOFT_DBLIB
	dbsetopt(dbproc, DBBUFFER, "100");
#else
	dbsetopt(dbproc, DBBUFFER, "100", 0);
#endif

	printf("creating table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	printf("insert\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}


	printf("select\n");
	dbcmd(dbproc, "select * from #dblib order by i");
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		printf("Was expecting a result set.");
		return 1;
	}

	for (i = 1; i <= dbnumcols(dbproc); i++) {
		printf("col %d is %s\n", i, dbcolname(dbproc, i));
	}

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
	dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);

	if (REG_ROW != dbnextrow(dbproc)) {
		fprintf(stderr, "dblib failed for %s:%d\n", __FILE__, __LINE__);
		return 1;
	}
	if (dbdatlen(dbproc, 2) != 6 || 0 != strcmp("ABCDEF", teststr)) {
		fprintf(stderr, "Expected |%s|, found |%s|\n", "ABCDEF", teststr);
		fprintf(stderr, "dblib failed for %s:%d\n", __FILE__, __LINE__);
		return 1;
	}

	if (REG_ROW != dbnextrow(dbproc)) {
		fprintf(stderr, "dblib failed for %s:%d\n", __FILE__, __LINE__);
		return 1;
	}
	if (dbdatlen(dbproc, 2) != 3 || 0 != strcmp("abc", teststr)) {
		fprintf(stderr, "Expected |%s|, found |%s|\n", "abc", teststr);
		fprintf(stderr, "dblib failed for %s:%d\n", __FILE__, __LINE__);
		return 1;
	}

	/* get again row 1 */
	dbgetrow(dbproc, 1);

	/* here length and string should be ok */
	if (dbdatlen(dbproc, 2) != 6 || 0 != strcmp("ABCDEF", teststr)) {
		fprintf(stderr, "Expected |%s|, found |%s|\n", "ABCDEF", teststr);
		fprintf(stderr, "dblib failed for %s:%d\n", __FILE__, __LINE__);
		return 1;
	}

	dbgetrow(dbproc, 2);
	if (dbdatlen(dbproc, 2) != 3 || 0 != strcmp("abc", teststr)) {
		fprintf(stderr, "Expected |%s|, found |%s|\n", "abc", teststr);
		fprintf(stderr, "dblib failed for %s:%d\n", __FILE__, __LINE__);
		return 1;
	}

	dbexit();

	printf("%s OK\n", __FILE__);
	return 0;
}
