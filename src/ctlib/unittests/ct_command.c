#include "common.h"

#include <cspublic.h>

/*
 * ct_command with CS_MORE option
 */
TEST_MAIN()
{
	int verbose = 0;
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;

	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	/* This should not crash, also only concatenation lead to no error */
	check_call(ct_command, (cmd, CS_LANG_CMD, "IF 0 = 1 ", CS_NULLTERM, CS_MORE));
	check_call(run_command, (cmd, "SELECT 'hello'"));

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
