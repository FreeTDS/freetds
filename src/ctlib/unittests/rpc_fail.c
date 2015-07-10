#include <config.h>

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STRING_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <ctpublic.h>
#include "common.h"

static const char *printable_ret(CS_RETCODE ret)
{
#define CASE(x) case x: return #x;
	switch (ret) {
	CASE(CS_STATUS_RESULT)
	CASE(CS_ROW_RESULT)
	CASE(CS_PARAM_RESULT)
	CASE(CS_CMD_SUCCEED)
	CASE(CS_MSG_RESULT)
	CASE(CS_CMD_DONE)
	CASE(CS_CMD_FAIL)
	default:
		return "Unknown";
	}
}

int
main(int argc, char *argv[])
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
	ret = try_ctlogin(&ctx, &conn, &cmd, verbose);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "Login failed\n");
		return 1;
	}

	if ((ret = ct_command(cmd, CS_RPC_CMD, "IDoNotExist", CS_NULLTERM, CS_NO_RECOMPILE)) != CS_SUCCEED) {
		fprintf(stderr, "ct_command(CS_RPC_CMD) failed");
		return 1;
	}

	if (ct_send(cmd) != CS_SUCCEED) {
		fprintf(stderr, "ct_send(RPC) failed");
		return 1;
	}

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		printf("ct_results returned %s\n", printable_ret(res_type));
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
			ret = ct_res_info(cmd, CS_MSGTYPE, (CS_VOID *) & msg_id, CS_UNUSED, NULL);
			if (ret != CS_SUCCEED) {
				fprintf(stderr, "ct_res_info(msg_id) failed");
				return 1;
			}
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

	ret = try_ctlogout(ctx, conn, cmd, verbose);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "Logout failed\n");
		return 1;
	}

	return 0;
}
