/*
 * Purpose: this will test what is returned from a batch of queries that do not return any rows
 * This is related to a bug first identified in PHPs PDO library https://bugs.php.net/bug.php?id=72969
 * Functions: dbbind dbcmd dbcolname dberrhandle dbisopt dbmsghandle dbnextrow dbnumcols dbopen dbresults dbsetlogintime dbsqlexec dbuse
 */

#include "common.h"

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	DBINT erc;

	RETCODE ret;
	int rowcount;
	int colcount;

	set_malloc_options();

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	/* Fortify_EnterScope(); */
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon as \"%s\"\n", USER);

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "batch_stmt_ins_upd");

	printf("About to open \"%s\"\n", SERVER);

	dbproc = dbopen(login, SERVER);
	if (!dbproc) {
		fprintf(stderr, "Unable to connect to %s\n", SERVER);
		return 1;
	}
	dbloginfree(login);

	printf("Using database \"%s\"\n", DATABASE);
	if (strlen(DATABASE)) {
		erc = dbuse(dbproc, DATABASE);
		assert(erc == SUCCEED);
	}

	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	/*
	 * This test is written to simulate how dblib is used in PDO
	 * functions are called in the same order they would be if doing
	 * PDO::query followed by some number of PDO::statement->nextRowset
	 */

	/*
	 * First, call everything that happens in PDO::query
	 * this will return the results of the CREATE TABLE statement
	 */
	dbcancel(dbproc);

	printf("using sql_cmd\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	ret = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", ret);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	/* check the results of the create table statement */
	assert(ret == SUCCEED);
	assert(rowcount == -1);
	assert(colcount == 0);

	/* now simulate calling nextRowset() for each remaining statement in our batch */

	/*
	 * INSERT
	 */
	ret = dbnextrow(dbproc);
	assert(ret == NO_MORE_ROWS);

	ret = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", ret);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(ret == SUCCEED);
	assert(rowcount == 3);
	assert(colcount == 0);

	/*
	 * UPDATE
	 */
	ret = dbnextrow(dbproc);
	assert(ret == NO_MORE_ROWS);

	ret = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", ret);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(ret == SUCCEED);
	assert(rowcount == 3);
	assert(colcount == 0);

	/*
	 * INSERT
	 */
	ret = dbnextrow(dbproc);
	assert(ret == NO_MORE_ROWS);

	ret = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", ret);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(ret == SUCCEED);
	assert(rowcount == 1);
	assert(colcount == 0);

	/*
	 * DROP
	 */
	ret = dbnextrow(dbproc);
	assert(ret == NO_MORE_ROWS);

	ret = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", ret);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(ret == SUCCEED);
	assert(rowcount == -1);
	assert(colcount == 0);

	/* Call one more time to be sure we get NO_MORE_RESULTS */
	ret = dbnextrow(dbproc);
	assert(ret == NO_MORE_ROWS);

	ret = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", ret);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(ret == NO_MORE_RESULTS);
	assert(rowcount == -1);
	assert(colcount == 0);

	dbexit();

	printf("%s OK\n", __FILE__);
	return 0;
}
