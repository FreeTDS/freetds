/* 
 * Purpose: Test dblib thread safety
 */

#include "common.h"

#ifdef TDS_HAVE_PTHREAD_MUTEX
#include <unistd.h>
#include <pthread.h>

static char software_version[] = "$Id: thread.c,v 1.9 2006-08-14 17:14:03 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int result = 0;
static int thread_count = 0;

#define ROWS 20
#define NUM_THREAD 10
#define NUM_LOOP 100

static void
set_failed(void)
{
	pthread_mutex_lock(&mutex);
	result = 1;
	pthread_mutex_unlock(&mutex);
}

static int
test(DBPROCESS *dbproc)
{
	int i;
	char teststr[1024];
	DBINT testint;


	/* fprintf(stdout, "select\n"); */
	dbcmd(dbproc, "select * from dblib_thread order by i");
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stdout, "Was expecting a result set.\n");
		set_failed();
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
		return 1;
	}

	dbcancel(dbproc);

	return 0;
}

static void *
thread_test(void * arg)
{
	int i;
	int num = (int) arg;
	DBPROCESS *dbproc;
	LOGINREC *login;

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "thread");

	dbproc = dbopen(login, SERVER);
	if (!dbproc) {
		dbloginfree(login);
		fprintf(stderr, "Unable to connect to %s\n", SERVER);
		set_failed();
		return NULL;
	}
	dbloginfree(login);

	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);

	pthread_mutex_lock(&mutex);
	++thread_count;
	pthread_mutex_unlock(&mutex);

	printf("thread %2d waiting for all threads to start\n", num+1);
	pthread_mutex_lock(&mutex);
	while (thread_count < NUM_THREAD) {
		pthread_mutex_unlock(&mutex);
		sleep(1);
		pthread_mutex_lock(&mutex);
	}
	pthread_mutex_unlock(&mutex);

	for (i = 1; i <= NUM_LOOP; ++i) {
		printf("thread %2d of %2d loop %d\n", num+1, NUM_THREAD, i);
		if (test(dbproc) || result != 0)
			break;
	}

	dbclose(dbproc);
	return NULL;
}

int
main(int argc, char **argv)
{
	int i;
	pthread_t th[NUM_THREAD];
	DBPROCESS *dbproc;
	LOGINREC *login;

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

	dbloginfree(login);

	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);

	fprintf(stdout, "Dropping table\n");
	dbcmd(dbproc, "if object_id('dblib_thread') is not null drop table dblib_thread");
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

	for (i = 0; i < NUM_THREAD; ++i) {
		int err = pthread_create(&th[i], NULL, thread_test, (void *) i);
		if (err != 0)
		{
			fprintf(stderr, "Error %d (%s) creating thread\n", err, strerror(err));
			return 1;
		}
		/* MSSQL rejects the connections if they come in too fast */
		sleep(1);
	}

	for (i = 0; i < NUM_THREAD; ++i) {
		pthread_join(th[i], NULL);
		fprintf(stdout, "thread: %d exited\n", i + 1);
	}

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
