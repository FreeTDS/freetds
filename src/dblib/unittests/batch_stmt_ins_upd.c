/*
 * Purpose: this will test what is returned from a batch of queries that do not return any rows
 * This is related to a bug first identified in PHPs PDO library https://bugs.php.net/bug.php?id=72969
 * Functions: dbbind dbcmd dbcolname dberrhandle dbisopt dbmsghandle dbnextrow dbnumcols dbopen dbresults dbsetlogintime dbsqlexec dbuse
 */

#include "common.h"

static char software_version[] = "$Id: batch_stmt_ins_upd.c,v 1.29 2016-11-17 16:00:00 jfarr Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };



int failed = 0;


int
main(int argc, char **argv)
{
  const int rows_to_add = 50;
  LOGINREC *login;
  DBPROCESS *dbproc;
  int i;
  char teststr[1024];
  DBINT testint, erc;

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

  RETCODE ret;
  int rowcount;
  int colcount;

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

  ret = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", ret);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  // check the results of the create table statement
  assert(ret == SUCCEED);
  assert(rowcount == -1);
  assert(colcount == 0);

  // now simulate calling nextRowset() for each remaining statement in our batch

  // --------------------------------------------------------------------------
  // INSERT
  // --------------------------------------------------------------------------
  ret = dbnextrow(dbproc);
  assert(ret == NO_MORE_ROWS);

  ret = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", ret);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(ret == SUCCEED);
  assert(rowcount == 3);
  assert(colcount == 0);

  // --------------------------------------------------------------------------
  // UPDATE
  // --------------------------------------------------------------------------
  ret = dbnextrow(dbproc);
  assert(ret == NO_MORE_ROWS);

  ret = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", ret);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(ret == SUCCEED);
  assert(rowcount == 3);
  assert(colcount == 0);

  // --------------------------------------------------------------------------
  // INSERT
  // --------------------------------------------------------------------------
  ret = dbnextrow(dbproc);
  assert(ret == NO_MORE_ROWS);

  ret = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", ret);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(ret == SUCCEED);
  assert(rowcount == 1);
  assert(colcount == 0);

  // --------------------------------------------------------------------------
  // DROP
  // --------------------------------------------------------------------------
  ret = dbnextrow(dbproc);
  assert(ret == NO_MORE_ROWS);

  ret = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", ret);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(ret == SUCCEED);
  assert(rowcount == -1);
  assert(colcount == 0);

  // Call one more time to be sure we get NO_MORE_RESULTS
  ret = dbnextrow(dbproc);
  assert(ret == NO_MORE_ROWS);

  ret = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", ret);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(ret == NO_MORE_RESULTS);
  assert(rowcount == -1);
  assert(colcount == 0);

  dbexit();

  fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
  return failed ? 1 : 0;
}
