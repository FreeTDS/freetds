/* Testing SQLCancel() */

#include "common.h"

/* TODO port to windows, use thread */

#include <signal.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_ALARM

static char sqlstate[SQL_SQLSTATE_SIZE + 1];

static void
getErrorInfo(SQLSMALLINT sqlhdltype, SQLHANDLE sqlhandle)
{
	SQLRETURN rcode = 0;
	SQLINTEGER naterror = 0;
	SQLCHAR msgtext[SQL_MAX_MESSAGE_LENGTH + 1];
	SQLSMALLINT msgtextl = 0;

	msgtext[0] = 0;
	rcode = SQLGetDiagRec((SQLSMALLINT) sqlhdltype,
			      (SQLHANDLE) sqlhandle,
			      (SQLSMALLINT) 1,
			      (SQLCHAR *) sqlstate,
			      (SQLINTEGER *) & naterror,
			      (SQLCHAR *) msgtext, (SQLSMALLINT) sizeof(msgtext), (SQLSMALLINT *) & msgtextl);
	sqlstate[sizeof(sqlstate)-1] = 0;
	fprintf(stderr, "Diagnostic info:\n");
	fprintf(stderr, "  SQL State: %s\n", sqlstate);
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
	CHK(SQLCancel, (Statement));
	printf(">>>> ... SQLCancel done\n");

	alarm(4);
	signal(SIGALRM, exit_forced);
}

int
main(int argc, char **argv)
{
	SQLRETURN rcode;

	use_odbc_version3 = 1;
	Connect();

	printf(">> Wait 5 minutes...\n");
	alarm(4);
	signal(SIGALRM, sigalrm_handler);
	rcode = SQLExecDirect(Statement, (SQLCHAR *) "WAITFOR DELAY '000:05:00'", SQL_NTS);
	alarm(0);
	if (rcode != SQL_ERROR) {
		fprintf(stderr, "SQLExecDirect should return error\n");
		return 1;
	}
	getErrorInfo(SQL_HANDLE_STMT, Statement);
	if (strcmp(sqlstate, "HY008") != 0) {
		fprintf(stderr, "Unexpected sql state returned\n");
		Disconnect();
		return 1;
	}
	printf(">> ...  done rcode = %d\n", rcode);

	ResetStatement();

	Command(Statement, "SELECT name FROM sysobjects WHERE 0=1");

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

