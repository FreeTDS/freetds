#include "common.h"

/* Testing: Server messages limit */
TEST_MAIN()
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int verbose = 0;
	int i;
	CS_INT num_msgs, totMsgs;
	CS_SERVERMSG server_message;

	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(ct_diag, (conn, CS_INIT, CS_UNUSED, CS_UNUSED, NULL));

	totMsgs = 4;

	check_call(ct_diag, (conn, CS_MSGLIMIT, CS_SERVERMSG_TYPE, CS_UNUSED, &totMsgs));

	printf("Maximum message limit is set to %d.\n", totMsgs);

	check_call(ct_diag, (conn, CS_STATUS, CS_SERVERMSG_TYPE, CS_UNUSED, &num_msgs));

	printf("Number of messages returned: %d\n", num_msgs);

	for (i = 0; i < num_msgs; i++) {

		check_call(ct_diag, (conn, CS_GET, CS_SERVERMSG_TYPE, i + 1, &server_message));

		servermsg_cb(ctx, conn, &server_message);

	}

	check_call(ct_diag, (conn, CS_CLEAR, CS_SERVERMSG_TYPE, CS_UNUSED, NULL));

	check_call(ct_diag, (conn, CS_STATUS, CS_SERVERMSG_TYPE, CS_UNUSED, &num_msgs));
	if (num_msgs != 0) {
		fprintf(stderr, "cs_diag(CS_CLEAR) failed there are still %d messages on queue\n", num_msgs);
		return 1;
	}

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
