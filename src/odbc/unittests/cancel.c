/* Testing SQLCancel() */

#include "common.h"

/* TODO port to windows, use thread */

#include <signal.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if defined(TDS_HAVE_PTHREAD_MUTEX) && HAVE_ALARM

#include <pthread.h>
#include "tdsthread.h"

static SQLTCHAR sqlstate[SQL_SQLSTATE_SIZE + 1];
static TDS_MUTEX_DECLARE(mtx);

static void
getErrorInfo(SQLSMALLINT sqlhdltype, SQLHANDLE sqlhandle)
{
	SQLINTEGER naterror = 0;
	SQLTCHAR msgtext[SQL_MAX_MESSAGE_LENGTH + 1];
	SQLSMALLINT msgtextl = 0;

	msgtext[0] = 0;
	SQLGetDiagRec(sqlhdltype,
			      (SQLHANDLE) sqlhandle,
			      1,
			      sqlstate,
			      &naterror,
			      msgtext, (SQLSMALLINT) ODBC_VECTOR_SIZE(msgtext), &msgtextl);
	sqlstate[ODBC_VECTOR_SIZE(sqlstate)-1] = 0;
	fprintf(stderr, "Diagnostic info:\n");
	fprintf(stderr, "  SQL State: %s\n", C(sqlstate));
	fprintf(stderr, "  SQL code : %d\n", (int) naterror);
	fprintf(stderr, "  Message  : %s\n", C(msgtext));
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
		TDS_MUTEX_LOCK(&mtx);
		if (exit_thread) {
			TDS_MUTEX_UNLOCK(&mtx);
			return NULL;
		}
		TDS_MUTEX_UNLOCK(&mtx);
	}

	exit_forced(0);
	return NULL;
}

static void
Test(int use_threads, int return_data)
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
	if (!return_data)
		CHKExecDirect(T("WAITFOR DELAY '000:05:00'"), SQL_NTS, "E");
	else
		odbc_command2("SELECT MAX(p1.k) FROM tab1 p1, tab1 p2, tab1 p3, tab1 p4", "E");

	TDS_MUTEX_LOCK(&mtx);
	exit_thread = 1;
	TDS_MUTEX_UNLOCK(&mtx);
	if (!use_threads) {
		alarm(0);
	} else {
		pthread_join(wait_thread, NULL);
	}
	getErrorInfo(SQL_HANDLE_STMT, odbc_stmt);
	if (strcmp(C(sqlstate), "HY008") != 0) {
		fprintf(stderr, "Unexpected sql state returned\n");
		odbc_disconnect();
		exit(1);
	}

	odbc_reset_statement();

	odbc_command("SELECT name FROM sysobjects WHERE 0=1");
}

int
main(int argc, char **argv)
{
	if (TDS_MUTEX_INIT(&mtx))
		return 1;

	if (odbc_read_login_info())
		exit(1);

	/*
	 * prepare our odbcinst.ini
	 * is better to do it before connect cause uniODBC cache INIs
	 * the name must be odbcinst.ini cause unixODBC accept only this name
	 */
	if (odbc_driver[0]) {
		FILE *f = fopen("odbcinst.ini", "w");

		if (f) {
			fprintf(f, "[FreeTDS]\nDriver = %s\nThreading = 0\n", odbc_driver);
			fclose(f);
			/* force iODBC */
			setenv("ODBCINSTINI", "./odbcinst.ini", 1);
			setenv("SYSODBCINSTINI", "./odbcinst.ini", 1);
			/* force unixODBC (only directory) */
			setenv("ODBCSYSINI", ".", 1);
		}
	}

	odbc_use_version3 = 1;
	odbc_connect();

	odbc_command("IF OBJECT_ID('tab1') IS NOT NULL DROP TABLE tab1");
	odbc_command("CREATE TABLE tab1 ( k INT, vc VARCHAR(200) )");

	printf(">> Creating tab1...\n");
	odbc_command("DECLARE @i INT\n"
		"SET @i = 1\n"
		"WHILE @i <= 2000 BEGIN\n"
		"INSERT INTO tab1 VALUES ( @i, 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' )\n"
		"SET @i = @i + 1\n"
		"END");
	while (CHKMoreResults("SNo") == SQL_SUCCESS)
		continue;
	printf(">> ...done.\n");

	odbc_reset_statement();

	Test(0, 0);
	Test(1, 0);
	Test(0, 1);
	Test(1, 1);

	odbc_command("DROP TABLE tab1");

	odbc_disconnect();
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

