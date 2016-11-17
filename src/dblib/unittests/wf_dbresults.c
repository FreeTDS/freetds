/*
 * Purpose:
 * Functions: dbbind dbcmd dbcolname dberrhandle dbisopt dbmsghandle dbnextrow dbnumcols dbopen dbresults dbsetlogintime dbsqlexec dbuse
 */

#include "common.h"

static char software_version[] = "$Id: wf_dbresults.c,v 1.29 2016-11-03 15:52:48 freddy77 Exp $";
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

  // First, call everything that happens in PDO::query
  // dblib_stmt.c pdo_dblib_stmt_execute
  //    pdo_dblib_stmt_cursor_closer will call this
  dbcancel(dbproc);

  //fprintf(stdout, "using dbcmd\n");
  //dbcmd(dbproc, "create table #wf_dbresults(id int);insert into #wf_dbresults values(1), (2), (3);update #wf_dbresults set id = 1;insert into #wf_dbresults values(2);drop table #wf_dbresults;");

  fprintf(stdout, "using sql_cmd\n");
  sql_cmd(dbproc);
  dbsqlexec(dbproc);

  //    pdo_dblib_stmt_next_rowset_no_cancel
  ret = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", ret);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  // end dblib_stmt.c pdo_dblib_stmt_execute
  assert(ret == SUCCEED);
  assert(rowcount == -1);
  assert(colcount == 0);

  // now simulate calling nextRowset()
  // this should return the results of the first INSERT

  // we don't expect any rows back from these statements
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

  // this should return the results of the UPDATE
  while (NO_MORE_ROWS != ret) {
    ret = dbnextrow(dbproc);

    if (FAIL == ret) {
      fprintf(stderr, "dbnextrows returned FAIL\n");
      return 1;
    }
  }

  ret = dbresults(dbproc);
  rowcount = DBCOUNT(dbproc);
  colcount = dbnumcols(dbproc);

  fprintf(stdout, "RETCODE: %d\n", ret);
  fprintf(stdout, "ROWCOUNT: %d\n", rowcount);
  fprintf(stdout, "COLCOUNT: %d\n\n", colcount);

  assert(ret == SUCCEED);
  assert(rowcount == 3);
  assert(colcount == 0);

  // this should return the results of the second INSERT
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

  // this should return the results of the DROP
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
