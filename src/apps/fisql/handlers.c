#include <stdio.h>
#include <string.h>
#include <sybfront.h>
#include <sybdb.h>
#include "handlers.h"

int global_errorlevel = -1;

int
err_handler(DBPROCESS *dbproc, int severity, int dberr, 
	    int oserr, char *dberrstr, char *oserrstr)
{
  if ((dbproc == NULL) || (DBDEAD(dbproc)))
  {
    return (INT_EXIT);
  }
  if (dberr != SYBESMSG)
  {
    fprintf(stdout, "DB-LIBRARY error:\n\t%s\n", dberrstr);
  }
  if (oserr != DBNOERR)
  {
    fprintf(stdout, "Operating-system error:\n\t%s\n", oserrstr);
  }
  return (INT_CANCEL);
}

int
msg_handler(DBPROCESS *dbproc, DBINT msgno, int msgstate, 
	    int severity, char *msgtext, char *srvname, 
	    char *procname, int line)
{
  if (severity > 0)
  {
    if ((global_errorlevel == -1) || (severity >= global_errorlevel))
    {
      fprintf(stdout, "Msg %ld, Level %d, State %d:\n",
	      (long) msgno, severity, msgstate);
    }
    if (global_errorlevel == -1)
    {
      if (strlen(srvname) > 0)
      {
	fprintf(stdout, "Server '%s', ", srvname);
      }
      if (strlen(procname) > 0)
      {
	fprintf(stdout, "Procedure '%s', ", procname);
      }
      if (line > 0)
      {
	fprintf(stdout, "Line %d:\n", line);
      }
    }
  }
  if (global_errorlevel == -1)
  {
    if ((severity > 0) || (msgno == 0))
    {
      fprintf(stdout, "%s\n", msgtext);
    }
  }
  return (0);
}

