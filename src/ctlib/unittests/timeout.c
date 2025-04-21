/*
 * Purpose: Test handling of timeouts with callbacks
 */

#include "common.h"
#include <freetds/macros.h>
#include <time.h>

static int ntimeouts = 0, ncancels = 0;
static int max_timeouts = 2, timeout_seconds = 2, cancel_timeout = 10;
static time_t start_time;
static const char sql[] =
	"select getdate() as 'begintime'\n"
	"waitfor delay '00:00:30'\n"
	"select getdate() as 'endtime'";

static int
on_interrupt(CS_CONNECTION *con TDS_UNUSED)
{
	printf("In on_interrupt, %ld seconds elapsed.\n", (long int) (time(NULL) - start_time));
	return CS_INT_CONTINUE;
}

static int
on_client_msg(CS_CONTEXT *ctx, CS_CONNECTION *con, CS_CLIENTMSG *errmsg)
{
	if (errmsg->msgnumber == 20003) {	/* TDSETIME */
		fprintf(stderr, "%d timeout(s) received in %ld seconds; ", ++ntimeouts, (long int) (time(NULL) - start_time));
		if (ntimeouts > max_timeouts) {
			if (++ncancels > 1) {
				fputs("could not time out cleanly;"
				      " breaking connection.\n", stderr);
				ncancels = 0;
				return CS_FAIL;
			}
			fputs("lost patience;"
			      " cancelling (allowing 10 seconds)\n", stderr);
			check_call(ct_con_props, (con, CS_SET, CS_TIMEOUT, &cancel_timeout, CS_UNUSED, NULL));
			return CS_FAIL;
		}
		fputs("continuing to wait.\n", stderr);
		return CS_SUCCEED;
	}
	return clientmsg_cb(ctx, con, errmsg);
}

static void
test(CS_CONNECTION *con, CS_COMMAND *cmd)
{
	CS_INT result_type = 0;
	CS_RETCODE ret;

	ntimeouts = 0;
	ncancels = 0;

	printf("Using %d-second query timeouts.\n", timeout_seconds);

	check_call(ct_con_props, (con, CS_SET, CS_TIMEOUT, &timeout_seconds, CS_UNUSED, NULL));

	/* Send something that will take a while to execute. */
	printf("Issuing a query that will take 30 seconds.\n");
	check_call(ct_command, (cmd, CS_LANG_CMD, (void *) sql, sizeof(sql) - 1, CS_UNUSED));

	start_time = time(NULL);	/* Track for reporting purposes. */
	ntimeouts = 0;
	check_call(ct_callback, (NULL, con, CS_SET, CS_CLIENTMSG_CB, &on_client_msg));
	check_call(ct_callback, (NULL, con, CS_SET, CS_INTERRUPT_CB, &on_interrupt));

	check_call(ct_send,(cmd));

	ret = ct_results(cmd, &result_type);
	if (ret == CS_SUCCEED) {
		fprintf(stderr, "Query unexpectedly succeeded, with result type %d.\n", result_type);
	} else {
		printf("Query failed as expected, with return code %d.\n", ret);
	}
}

TEST_MAIN()
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *con;
	CS_COMMAND *cmd;

	check_call(try_ctlogin_with_options, (argc, argv, &ctx, &con, &cmd, false));
	test(con, cmd);
	try_ctlogout(ctx, con, cmd, false);
	return 0;
}
