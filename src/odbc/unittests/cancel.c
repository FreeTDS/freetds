/* Testing SQLCancel() */

#include "common.h"

/* TODO port to windows, use thread */

#include <signal.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined(TDS_HAVE_PTHREAD_MUTEX) && HAVE_ALARM

#include <pthread.h>

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
	printf(">>>> SQLCancel() ...\n");
	CHKCancel("S");
	printf(">>>> ... SQLCancel done\n");

	alarm(4);
	signal(SIGALRM, exit_forced);
}

volatile int exit_thread;

static void *
wait_thread_proc(void * arg)
{
	int n;

	sleep(4);

	printf(">>>> SQLCancel() ...\n");
	CHKCancel("S");
	printf(">>>> ... SQLCancel done\n");
	
	for (n = 0; n < 4; ++n) {
		sleep(1);
		if (exit_thread)
			return NULL;
	}

	exit_forced(0);
	return NULL;
}

static void
Test(int use_threads)
{
	pthread_t wait_thread;

	printf("testing with %s\n", use_threads ? "threads" : "signals");
	printf(">> Wait 5 minutes...\n");
	if (!use_threads) {
		alarm(4);
		signal(SIGALRM, sigalrm_handler);
	} else {
		int err;

		exit_thread = 0;
		err = pthread_create(&wait_thread, NULL, wait_thread_proc, NULL);
		if (err != 0) {
			perror("pthread_create");
			exit(1);
		}
	}
	CHKExecDirect((SQLCHAR *) "WAITFOR DELAY '000:05:00'", SQL_NTS, "E");
	exit_thread = 1;
	if (!use_threads) {
		alarm(0);
	} else {
		pthread_join(wait_thread, NULL);
	}
	getErrorInfo(SQL_HANDLE_STMT, Statement);
	if (strcmp(sqlstate, "HY008") != 0) {
		fprintf(stderr, "Unexpected sql state returned\n");
		Disconnect();
		exit(1);
	}

	ResetStatement();

	Command("SELECT name FROM sysobjects WHERE 0=1");
}

int
main(int argc, char **argv)
{
	if (read_login_info())
		exit(1);

	/*
	 * prepare our odbcinst.ini
	 * is better to do it before connect cause uniODBC cache INIs
	 * the name must be odbcinst.ini cause unixODBC accept only this name
	 */
	if (DRIVER[0]) {
		FILE *f = fopen("odbcinst.ini", "w");

		if (f) {
			fprintf(f, "[FreeTDS]\nDriver = %s\nThreading = 0\n", DRIVER);
			fclose(f);
			/* force iODBC */
			setenv("ODBCINSTINI", "./odbcinst.ini", 1);
			setenv("SYSODBCINSTINI", "./odbcinst.ini", 1);
			/* force unixODBC (only directory) */
			setenv("ODBCSYSINI", ".", 1);
		}
	}

	use_odbc_version3 = 1;
	Connect();

	Test(0);
	Test(1);

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

