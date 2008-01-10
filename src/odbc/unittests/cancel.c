/* Testing SQLCancel() */

#include "common.h"
#include <signal.h>

#ifdef HAVE_ALARM

#define CHECK_RCODE(t,h,m) \
   if ( rcode != SQL_NO_DATA \
     && rcode != SQL_SUCCESS \
     && rcode != SQL_SUCCESS_WITH_INFO  \
     && rcode != SQL_NEED_DATA ) { \
      fprintf(stderr,"Error %d at: %s\n",rcode,m); \
      getErrorInfo(t,h); \
      exit(1); \
   }

static SQLCHAR sqlstate[SQL_SQLSTATE_SIZE + 1];

static void
getErrorInfo(SQLSMALLINT sqlhdltype, SQLHANDLE sqlhandle)
{
	SQLRETURN rcode = 0;
	SQLINTEGER naterror = 0;
	SQLCHAR msgtext[SQL_MAX_MESSAGE_LENGTH + 1];
	SQLSMALLINT msgtextl = 0;

	rcode = SQLGetDiagRec((SQLSMALLINT) sqlhdltype,
			      (SQLHANDLE) sqlhandle,
			      (SQLSMALLINT) 1,
			      (SQLCHAR *) sqlstate,
			      (SQLINTEGER *) & naterror,
			      (SQLCHAR *) msgtext, (SQLSMALLINT) sizeof(msgtext), (SQLSMALLINT *) & msgtextl);
	fprintf(stderr, "Diagnostic info:\n");
	fprintf(stderr, "  SQL State: %s\n", (char *) sqlstate);
	fprintf(stderr, "  SQL code : %d\n", (int) naterror);
	fprintf(stderr, "  Message  : %s\n", (char *) msgtext);
}

static void
exit_forced(int s)
{
	exit(1);
}

static void
sigalrm_handler(int s)
{
	SQLRETURN rcode;

	printf(">>>> SQLCancel() ...\n");
	rcode = SQLCancel(Statement);
	printf(">>>> ... SQLCancel done rcode = %d\n", rcode);
	CHECK_RCODE(SQL_HANDLE_STMT, Statement, "SQLCancel failed");

	alarm(4);
	signal(SIGALRM, exit_forced);
}

int
main(int argc, char **argv)
{
	SQLRETURN rcode;

	use_odbc_version3 = 1;
	Connect();

	printf(">> Select tab1...\n");
	alarm(4);
	signal(SIGALRM, sigalrm_handler);
	rcode = SQLExecDirect(Statement, (SQLCHAR *) "WAITFOR DELAY '000:05:00'", SQL_NTS);
	if (rcode != SQL_ERROR) {
		fprintf(stderr, "SQLExecDirect should return error\n");
		return 1;
	}
	getErrorInfo(SQL_HANDLE_STMT, Statement);
	if (strcmp(sqlstate, "HY008") != 0) {
		fprintf(stderr, "Unexpected sql state returned\n");
		return 1;
	}
	printf(">> ...  done rcode = %d\n", rcode);
	signal(SIGINT, NULL);

	Disconnect();
	return 0;
}

#else
int
main(void)
{
	printf("Not possible for this platform.\n");
	return 0;
}
#endif

