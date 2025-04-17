#include "common.h"

#include <freetds/time.h>

#include <sys/types.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <signal.h>

#include <freetds/macros.h>

#if defined(HAVE_ALARM) && defined(HAVE_SETITIMER)

/* protos */
static int do_fetch(CS_COMMAND * cmd, int *cnt);

/* Globals */
static volatile CS_COMMAND *g_cmd = NULL;

static void
catch_alrm(int sig_num TDS_UNUSED)
{
	signal(SIGALRM, catch_alrm);

	printf("- SIGALRM\n");

	/* Cancel current command */
	if (g_cmd)
		ct_cancel(NULL, (CS_COMMAND *) g_cmd, CS_CANCEL_ATTN);

	fflush(stdout);
}

/* Testing: Test asynchronous ct_cancel() */
TEST_MAIN()
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int i, verbose = 0, cnt = 0;

	CS_RETCODE ret;
	CS_INT result_type;

	struct itimerval timer;
	char query[1024];

	unsigned clock = 200000;

	printf("%s: Check asynchronous called ct_cancel()\n", __FILE__);

	/* disable dump for this test, there are some issues with concurrent
	 * execution of this test if logging is enabled. */
	unsetenv("TDSDUMP");
	unsetenv("TDSDUMPCONFIG");

	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	/* Create needed tables */
	check_call(run_command, (cmd, "CREATE TABLE #t0010 (id int, col1 varchar(255))"));

	for (i = 0; i < 10; i++) {
		sprintf(query, "INSERT #t0010 (id, col1) values (%d, 'This is field no %d')", i, i);

		check_call(run_command, (cmd, query));
	}

	/* Set SIGALRM signal handler */
	signal(SIGALRM, catch_alrm);

	for (;;) {
		/* TODO better to use alarm AFTER ct_send ?? */
		/* Set timer */
		timer.it_interval.tv_sec = 0;
		timer.it_interval.tv_usec = clock;
		timer.it_value.tv_sec = 0;
		timer.it_value.tv_usec = clock;
		if (0 != setitimer(ITIMER_REAL, &timer, NULL)) {
			fprintf(stderr, "Could not set realtime timer.\n");
			return 1;
		}

		/* Issue a command returning many rows */
		check_call(ct_command, 
			   (cmd, CS_LANG_CMD, "SELECT * FROM #t0010 t1, #t0010 t2, #t0010 t3, #t0010 t4", CS_NULLTERM, CS_UNUSED));

		check_call(ct_send, (cmd));

		/* Save a global reference for the interrupt handler */
		g_cmd = cmd;

		while ((ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
			printf("More results?...\n");
			if (result_type == CS_STATUS_RESULT)
				continue;

			switch ((int) result_type) {
			case CS_ROW_RESULT:
				printf("do_fetch() returned: %d\n", do_fetch(cmd, &cnt));
				break;
			}
		}

		/* We should not have received all rows, as the alarm signal cancelled it... */
		if (cnt < 10000)
			break;

		if (clock <= 5000) {
			fprintf(stderr, "All rows read, this may not occur.\n");
			return 1;
		}
		g_cmd = NULL;
		clock /= 2;
	}

	/* Remove timer */
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 0;
	if (0 != setitimer(ITIMER_REAL, &timer, NULL)) {
		fprintf(stderr, "Could not remove realtime timer.\n");
		return 1;
	}

	/*
	 * Issue another command, this will be executed after a ct_cancel, 
	 * to test if wire state is consistent 
	 */
	check_call(ct_command, (cmd, CS_LANG_CMD, "SELECT * FROM #t0010 t1, #t0010 t2, #t0010 t3", CS_NULLTERM, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		printf("More results?...\n");
		if (result_type == CS_STATUS_RESULT)
			continue;

		switch ((int) result_type) {
		case CS_ROW_RESULT:
			printf("do_fetch() returned: %d\n", do_fetch(cmd, &cnt));
			break;
		}
	}

	if (1000 != cnt) {
		/* This time, all rows must have been received */
		fprintf(stderr, "Incorrect number of rows read.\n");
		return 1;
	}

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	printf("%s: asynchronous cancel test: PASSED\n", __FILE__);

	return 0;
}

static int
do_fetch(CS_COMMAND * cmd, int *cnt)
{
	CS_INT count, row_count = 0;
	CS_RETCODE ret;

	while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED) {
		/* printf ("ct_fetch() == CS_SUCCEED\n"); */
		row_count += count;
	}

	(*cnt) = row_count;
	if (ret == CS_ROW_FAIL) {
		fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
		return 1;
	} else if (ret == CS_END_DATA) {
		printf("do_fetch retrieved %d rows\n", row_count);
		return 0;
	} else if (ret == CS_CMD_FAIL) {
		printf("do_fetch(): command aborted after receiving %d rows\n", row_count);
		return 0;
	} else {
		fprintf(stderr, "ct_fetch() unexpected return %d on row %d.\n", ret, row_count);
		return 1;
	}
}

#else

TEST_MAIN()
{
	return 0;
}
#endif

