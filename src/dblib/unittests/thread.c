/* 
 * Purpose: Log in, create a table, insert a few rows, select them, and log out.   
 * Functions: dbbind dbcmd dbcolname dberrhandle dbisopt dbmsghandle dbnextrow dbnumcols dbopen dbresults dbsetlogintime dbsqlexec dbuse 
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <sqlfront.h>
#include <sqldb.h>

#ifdef TDS_HAVE_PTHREAD_MUTEX

#include <unistd.h>
#include "tdsthread.h"

#include "common.h"

static char software_version[] = "$Id: thread.c,v 1.1 2005-05-17 12:10:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static TDS_MUTEX_DECLARE(mutex);

static LOGINREC *login = NULL;
static volatile int result = 0;
static volatile int thread_count = 0;

#define ROWS 20
#define NUM_THREAD 10
#define NUM_LOOP 100

static void
set_failed(void)
{
	TDS_MUTEX_LOCK(&mutex);
	result = 1;
	TDS_MUTEX_UNLOCK(&mutex);
}

static int
test(void)
{
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	DBINT testint;
	LOGINREC *login;

	dbinit();

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "thread");

	dbproc = dbopen(login, SERVER);
	if (!dbproc) {
		dbloginfree(login);
		fprintf(stderr, "Unable to connect to %s\n", SERVER);
		if (thread_count <= 5)
			set_failed();
		return 1;
	}
	dbloginfree(login);

	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);

	/* fprintf(stdout, "select\n"); */
	dbcmd(dbproc, "select * from dblib_thread(nolock) order by i");
	dbsqlexec(dbproc);


	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stdout, "Was expecting a result set.\n");
		set_failed();
		dbclose(dbproc);
		return 1;
	}

	if (SUCCEED != dbbind(dbproc, 1, INTBIND, -1, (BYTE *) & testint)) {
		fprintf(stderr, "Had problem with bind\n");
		abort();
	}
	if (SUCCEED != dbbind(dbproc, 2, STRINGBIND, -1, (BYTE *) teststr)) {
		fprintf(stderr, "Had problem with bind\n");
		abort();
	}

	for (i = 0; i < ROWS; i++) {
		char expected[64];

		sprintf(expected, "row %d", i);

		memset(teststr, 'x', sizeof(teststr));
		teststr[0] = 0;
		teststr[sizeof(teststr) - 1] = 0;
		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			set_failed();
			dbclose(dbproc);
			return 1;
		}
		if (testint != i) {
			fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i, (int) testint);
			abort();
		}
		if (0 != strncmp(teststr, expected, strlen(expected))) {
			fprintf(stdout, "Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
			abort();
		}
		/* printf("Read a row of data -> %d |%s|\n", (int) testint, teststr); */
	}


	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		fprintf(stderr, "Was expecting no more rows\n");
		set_failed();
		dbclose(dbproc);
		return 1;
	}

	dbclose(dbproc);

	dbexit();

	return 0;
}

static void *
thread_test(void * arg)
{
	int i;
	int num = (int) arg;

	for (i = 1; i <= NUM_LOOP; ++i) {
		printf("thread %2d of %2d loop %d\n", num, NUM_THREAD, i);
		if (test() || result != 0)
			break;
	}

	TDS_MUTEX_LOCK(&mutex);
	--thread_count;
	TDS_MUTEX_UNLOCK(&mutex);

	return NULL;
}

int
main(int argc, char **argv)
{
	int i;
	pthread_t th;
	DBPROCESS *dbproc;

	read_login_info(argc, argv);

	fprintf(stdout, "Start\n");

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "thread");

	fprintf(stdout, "About to open \"%s\"\n", SERVER);

	dbproc = dbopen(login, SERVER);
	if (!dbproc) {
		fprintf(stderr, "Unable to connect to %s\n", SERVER);
		return 1;
	}

	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);

	fprintf(stdout, "Dropping table\n");
	dbcmd(dbproc, "drop table dblib_thread");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	fprintf(stdout, "creating table\n");
	dbcmd(dbproc, "create table dblib_thread (i int not null, s char(10) not null)");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	fprintf(stdout, "insert\n");
	for (i = 0; i < ROWS; i++) {
		char cmd[128];

		sprintf(cmd, "insert into dblib_thread values (%d, 'row %d')", i, i);
		dbcmd(dbproc, cmd);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) == SUCCEED) {
			/* nop */
		}
	}

	for (i = 1; i <= NUM_THREAD; ++i) {
		TDS_MUTEX_LOCK(&mutex);
		++thread_count;
		TDS_MUTEX_UNLOCK(&mutex);
		if (pthread_create(&th, NULL, thread_test, (void *) i) != 0) {
			fprintf(stderr, "Error creating thread\n");
			return 1;
		}
		pthread_detach(th);
		usleep(20000);
	}

	for (i = 1; thread_count > 0; ++i) {
		printf("loop %d thread count %d... waiting\n", i, thread_count);
		sleep(2);
	}

	dbloginfree(login);

	fprintf(stdout, "Dropping table\n");
	dbcmd(dbproc, "drop table dblib_thread");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		/* nop */
	}

	dbexit();

	return result;
}

#else /* !TDS_HAVE_PTHREAD_MUTEX */

int
main(int argc, char **argv)
{
	return 0;
}
#endif

