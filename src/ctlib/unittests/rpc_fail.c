#include "common.h"

TEST_MAIN()
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int verbose = 0;
	CS_RETCODE ret;
	CS_INT res_type;
	CS_SMALLINT msg_id;
	int got_failure = 0;
	int got_done = 0;
	CS_INT rows_read;

	printf("%s: submit a not existing stored procedure\n", __FILE__);
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(ct_command, (cmd, CS_RPC_CMD, "IDoNotExist", CS_NULLTERM, CS_NO_RECOMPILE));

	check_call(ct_send, (cmd));

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		printf("ct_results returned %s\n", res_type_str(res_type));
		switch ((int) res_type) {
		case CS_STATUS_RESULT:
			while (ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &rows_read) == CS_SUCCEED)
				continue;
			break;

		case CS_CMD_SUCCEED:
			fprintf(stderr, "a success was not expected from ct_results.\n");
			return 1;

		case CS_ROW_RESULT:
		case CS_PARAM_RESULT:
			fprintf(stderr, "Only status results are expected\n");
			return 1;

		case CS_MSG_RESULT:
			check_call(ct_res_info, (cmd, CS_MSGTYPE, (CS_VOID *) & msg_id, CS_UNUSED, NULL));
			printf("ct_result returned CS_MSG_RESULT where msg id = %d.\n", msg_id);
			break;

		case CS_CMD_DONE:
			got_done = 1;
			break;

		case CS_CMD_FAIL:
			got_failure = 1;
			break;

		default:
			fprintf(stderr, "ct_results returned unexpected result type.\n");
			return 1;
		}
	}

	if (!got_failure) {
		fprintf(stderr, "a failure was expected from ct_results.\n");
		return 1;
	}
	if (!got_done) {
		fprintf(stderr, "a done result was expected from ct_results.\n");
		return 1;
	}

	switch ((int) ret) {
	case CS_END_RESULTS:
		break;

	case CS_FAIL:
		fprintf(stderr, "ct_results failed.");
		return 1;

	default:
		fprintf(stderr, "ct_results returned unexpected result type.");
		return 1;
	}

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
