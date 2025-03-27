/*
 * Purpose: this will test what is returned from a batch of queries, some that return rows and some that do not
 * This is related to a bug first identified in PHPs PDO library https://bugs.php.net/bug.php?id=72969
 * Functions: dbbind dbcmd dbcolname dberrhandle dbisopt dbmsghandle dbnextrow dbnumcols dbopen dbresults dbsetlogintime dbsqlexec dbuse
 */

#include "common.h"

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	DBINT erc;

	RETCODE results_retcode;
	int rowcount;
	int colcount;
	int row_retcode;

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
	DBSETLAPP(login, "batch_stmt_ins_sel");

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

	results_retcode = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("** CREATE TABLE **\n");
	printf("RETCODE: %d\n", results_retcode);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	/* check that the results correspond to the create table statement */
	assert(results_retcode == SUCCEED);
	assert(rowcount == -1);
	assert(colcount == 0);

	/* now simulate calling nextRowset() for each remaining statement in our batch */

	/*
	 * INSERT
	 */
	printf("** INSERT **\n");

	/* there shouldn't be any rows in this resultset yet, it's still from the CREATE TABLE */
	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n", results_retcode);
	assert(row_retcode == NO_MORE_ROWS);

	results_retcode = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", results_retcode);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(results_retcode == SUCCEED);
	assert(rowcount == 3);
	assert(colcount == 0);

	/*
	 * SELECT
	 */
	printf("** SELECT **\n");

	/* the rowset is still from the INSERT and should have no rows */
	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n", results_retcode);
	assert(row_retcode == NO_MORE_ROWS);

	results_retcode = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", results_retcode);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(results_retcode == SUCCEED);
	assert(rowcount == -1);
	assert(colcount == 1);

	/* now we expect to find three rows in the rowset */
	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n", row_retcode);
	assert(row_retcode == REG_ROW);
	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n", row_retcode);
	assert(row_retcode == REG_ROW);
	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n\n", row_retcode);
	assert(row_retcode == REG_ROW);

	/*
	 * UPDATE
	 */
	printf("** UPDATE **\n");

	/* check that there are no rows left, then we'll get the results from the UPDATE */
	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n", row_retcode);
	assert(row_retcode == NO_MORE_ROWS);

	results_retcode = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", results_retcode);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(results_retcode == SUCCEED);
	assert(rowcount == 3);
	/*assert(colcount == 0); TODO: why does an update get a column? */

	/*
	 * SELECT
	 */
	printf("** SELECT **\n");

	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n", row_retcode);
	assert(row_retcode == NO_MORE_ROWS);

	results_retcode = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", results_retcode);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(results_retcode == SUCCEED);
	assert(rowcount == -1);
	assert(colcount == 1);

	/* now we expect to find three rows in the rowset again */
	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n", row_retcode);
	assert(row_retcode == REG_ROW);
	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n", row_retcode);
	assert(row_retcode == REG_ROW);
	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n\n", row_retcode);
	assert(row_retcode == REG_ROW);

	/*
	 * DROP
	 */
	printf("** DROP **\n");

	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n", row_retcode);
	assert(row_retcode == NO_MORE_ROWS);

	results_retcode = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", results_retcode);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(results_retcode == SUCCEED);
	assert(rowcount == -1);
	/* assert(colcount == 0); */

	/* Call one more time to be sure we get NO_MORE_RESULTS */
	row_retcode = dbnextrow(dbproc);
	printf("dbnextrow retcode: %d\n", row_retcode);
	assert(row_retcode == NO_MORE_ROWS);

	results_retcode = dbresults(dbproc);
	rowcount = DBCOUNT(dbproc);
	colcount = dbnumcols(dbproc);

	printf("RETCODE: %d\n", results_retcode);
	printf("ROWCOUNT: %d\n", rowcount);
	printf("COLCOUNT: %d\n\n", colcount);

	assert(results_retcode == NO_MORE_RESULTS);
	assert(rowcount == -1);
	/* assert(colcount == 0); */

	dbexit();

	printf("%s OK\n", __FILE__);
	return 0;
}
