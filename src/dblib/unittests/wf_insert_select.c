/*
 * Purpose:
 * Functions: dbbind dbcmd dbcolname dberrhandle dbisopt dbmsghandle dbnextrow dbnumcols dbopen dbresults dbsetlogintime dbsqlexec dbuse
 */

#include "common.h"

static char software_version[] = "$Id: wf_insert_select.c,v 1.29 2016-11-03 15:52:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };



int failed = 0;


int
main(int argc, char **argv)
{
  LOGINREC *login;
  DBPROCESS *dbproc;
  int i;
  DBINT erc;

  set_malloc_options();

  read_login_info(argc, argv);
  if (argc > 1) {
    argc -= optind;
    argv += optind;
  }

  fprintf(stdout, "Starting %s\n", argv[0]);

  /* Fortify_EnterScope(); */
  dbinit();

  dberrhandle(syb_err_handler);
  dbmsghandle(syb_msg_handler);

  fprintf(stdout, "About to logon as \"%s\"\n", USER);

  login = dblogin();
  DBSETLPWD(login, PASSWORD);
  DBSETLUSER(login, USER);
  DBSETLAPP(login, "wf_dbresults");

  if (argc > 1) {
    printf("server and login timeout overrides (%s and %s) detected\n", argv[0], argv[1]);
    strcpy(SERVER, argv[0]);
    i = atoi(argv[1]);
    if (i) {
      i = dbsetlogintime(i);
      printf("dbsetlogintime returned %s.\n", (i == SUCCEED)? "SUCCEED" : "FAIL");
    }
  }

  fprintf(stdout, "About to open \"%s\"\n", SERVER);

  dbproc = dbopen(login, SERVER);
  if (!dbproc) {
    fprintf(stderr, "Unable to connect to %s\n", SERVER);
    return 1;
  }
  dbloginfree(login);

  fprintf(stdout, "Using database \"%s\"\n", DATABASE);
  if (strlen(DATABASE)) {
    erc = dbuse(dbproc, DATABASE);
    assert(erc == SUCCEED);
  }

  RETCODE results_retcode;
  int rowcount;
  int colcount;
  int row_retcode;

  /*
  * This test is written to simulate how dblib is used in PDO
  * functions are called in the same order they would be if doing
  * PDO::query followed by some number of PDO::statement->nextRowset
  */

  // First, call everything that happens in PDO::query
  // this will return the results of the CREATE TABLE statement
  dbcancel(dbproc);

  fprintf(stdout, "using sql_cmd\n");
  sql_cmd(dbproc);
  dbsqlexec(dbproc);

  results_retcode = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "** CREATE TABLE **\n");
  fprintf(stdout, "RETCODE: %d\n", results_retcode);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  // check that the results correspond to the create table statement
  assert(results_retcode == SUCCEED);
  assert(rowcount == -1);
  assert(colcount == 0);

  // now simulate calling nextRowset() for each remaining statement in our batch

  // --------------------------------------------------------------------------
  // INSERT
  // --------------------------------------------------------------------------
  fprintf(stdout, "** INSERT **\n");

  // there shouldn't be any rows in this resultset yet, it's still from the CREATE TABLE
  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n", results_retcode);
  assert(row_retcode == NO_MORE_ROWS);

  results_retcode = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", results_retcode);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(results_retcode == SUCCEED);
  assert(rowcount == 3);
  assert(colcount == 0);

  // --------------------------------------------------------------------------
  // SELECT
  // --------------------------------------------------------------------------
  fprintf(stdout, "** SELECT **\n");

  // the rowset is still from the INSERT and should have no rows
  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n", results_retcode);
  assert(row_retcode == NO_MORE_ROWS);

  results_retcode = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", results_retcode);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(results_retcode == SUCCEED);
  assert(rowcount == -1);
  assert(colcount == 1);

  // now we expect to find three rows in the rowset
  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n", row_retcode);
  assert(row_retcode == REG_ROW); // 4040 corresponds to TDS_ROW_RESULT in freetds/tds.h
  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n", row_retcode);
  assert(row_retcode == REG_ROW);
  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n\n", row_retcode);
  assert(row_retcode == REG_ROW);

  // --------------------------------------------------------------------------
  // UPDATE
  // --------------------------------------------------------------------------
  fprintf(stdout, "** UPDATE **\n");

  // check that there are no rows left, then we'll get the results from the UPDATE
  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n", row_retcode);
  assert(row_retcode == NO_MORE_ROWS);

  results_retcode = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", results_retcode);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(results_retcode == SUCCEED);
  assert(rowcount == 3);
  //assert(colcount == 0); // TODO: why does an update get a column?

  // --------------------------------------------------------------------------
  // SELECT
  // --------------------------------------------------------------------------
  fprintf(stdout, "** SELECT **\n");

  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n", row_retcode);
  assert(row_retcode == NO_MORE_ROWS);

  results_retcode = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", results_retcode);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(results_retcode == SUCCEED);
  assert(rowcount == -1);
  assert(colcount == 1);

  // now we expect to find three rows in the rowset again
  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n", row_retcode);
  assert(row_retcode == REG_ROW);
  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n", row_retcode);
  assert(row_retcode == REG_ROW);
  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n\n", row_retcode);
  assert(row_retcode == REG_ROW);

  // --------------------------------------------------------------------------
  // DROP
  // --------------------------------------------------------------------------
  fprintf(stdout, "** DROP **\n");

  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n", row_retcode);
  assert(row_retcode == NO_MORE_ROWS);

  results_retcode = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", results_retcode);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(results_retcode == SUCCEED);
  assert(rowcount == -1);
  //assert(colcount == 1);

  // Call one more time to be sure we get NO_MORE_RESULTS
  row_retcode = dbnextrow(dbproc);
  fprintf(stdout, "dbnextrow retcode: %d\n", row_retcode);
  assert(row_retcode == NO_MORE_ROWS);

  results_retcode = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", results_retcode);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(results_retcode == NO_MORE_RESULTS);
  assert(rowcount == -1);
  //assert(colcount == 0);

  dbexit();

  fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
  return failed ? 1 : 0;
}
