/*
 * Try usage of callbacks to get errors and messages from library.
 */
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctpublic.h>
#include "common.h"

static void test_ct_callback(void);
static void test_ct_res_info(void);
static void test_ct_send(void);

static void
report_wrong_error(void)
{
	fprintf(stderr, "Wrong error type %d number %d (%#x)\n",
		ct_last_message.type, ct_last_message.number, ct_last_message.number);
	exit(1);
}

static CS_CONTEXT *ctx;
static CS_CONNECTION *conn;
static CS_COMMAND *cmd;

int
main(void)
{
	int verbose = 1;

	printf("%s: Testing message callbacks\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(cs_config, (ctx, CS_SET, CS_MESSAGE_CB, (CS_VOID*) cslibmsg_cb, CS_UNUSED, NULL));

	/* set different callback for the connection */
	check_call(ct_callback, (NULL, conn, CS_SET, CS_CLIENTMSG_CB, (CS_VOID*) clientmsg_cb2));

	test_ct_callback();
	test_ct_res_info();
	test_ct_send();

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	if (verbose) {
		printf("Test succeeded\n");
	}
	return 0;
}

static void
_check_fail(const char *name, CS_RETCODE ret, int line)
{
	if (ret != CS_FAIL) {
		fprintf(stderr, "%s():%d: succeeded\n", name, line);
		exit(1);
	}
}
#define check_fail(func, args) do { \
	ct_reset_last_message(); \
	_check_fail(#func, func args, __LINE__); \
} while(0)

static void
test_ct_callback(void)
{
	/* this should fail, context or connection should be not NULL */
	check_fail(ct_callback, (NULL, NULL, CS_SET, CS_SERVERMSG_CB, servermsg_cb));
	if (ct_last_message.type != CTMSG_NONE)
		report_wrong_error();

	/* this should fail, context and connection cannot be both not NULL */
	check_fail(ct_callback, (ctx, conn, CS_SET, CS_SERVERMSG_CB, servermsg_cb));
	if (ct_last_message.type != CTMSG_CLIENT2 || ct_last_message.number != 0x01010133)
		report_wrong_error();

	/* this should fail, invalid action */
	check_fail(ct_callback, (ctx, NULL, 3, CS_SERVERMSG_CB, servermsg_cb));
	if (ct_last_message.type != CTMSG_CLIENT || ct_last_message.number != 0x01010105
	    || !strstr(ct_last_message.text, "action"))
		report_wrong_error();

	/* this should fail, invalid action */
	check_fail(ct_callback, (NULL, conn, 3, CS_SERVERMSG_CB, servermsg_cb));
	if (ct_last_message.type != CTMSG_CLIENT2 || ct_last_message.number != 0x01010105
	    || !strstr(ct_last_message.text, "action"))
		report_wrong_error();

	/* this should fail, invalid type */
	check_fail(ct_callback, (ctx, NULL, CS_SET, 20, servermsg_cb));
	if (ct_last_message.type != CTMSG_CLIENT || ct_last_message.number != 0x01010105
	    || !strstr(ct_last_message.text, "type"))
		report_wrong_error();

	/* this should fail, invalid type */
	check_fail(ct_callback, (NULL, conn, CS_SET, 20, servermsg_cb));
	if (ct_last_message.type != CTMSG_CLIENT2 || ct_last_message.number != 0x01010105
	    || !strstr(ct_last_message.text, "type"))
		report_wrong_error();
}

static void
test_ct_res_info(void)
{
	CS_RETCODE ret;
	CS_INT result_type;
	CS_INT num_cols;
	CS_INT count;

	check_call(ct_command, (cmd, CS_LANG_CMD, "SELECT 'hi' AS greeting", CS_NULLTERM, CS_UNUSED));
	check_call(ct_send, (cmd));

	while ((ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch (result_type) {
		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;
		case CS_ROW_RESULT:
			/* this should fail, invalid number */
			check_fail(ct_res_info, (cmd, 1234, &num_cols, CS_UNUSED, NULL));
			if (ct_last_message.type != CTMSG_CLIENT2 || ct_last_message.number != 0x01010105
			    || !strstr(ct_last_message.text, "operation"))
				report_wrong_error();

			while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
				continue;

			if (ret != CS_END_DATA) {
				fprintf(stderr, "ct_fetch() unexpected return %d.\n", (int) ret);
				exit(1);
			}
			break;
		default:
			fprintf(stderr, "ct_results() unexpected result_type %d.\n", (int) result_type);
			exit(1);
		}
	}
	if (ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results() unexpected return %d.\n", (int) ret);
		exit(1);
	}
}

static void
test_ct_send(void)
{
	/* reset command to idle state */
	check_call(ct_cmd_drop, (cmd));
	check_call(ct_cmd_alloc, (conn, &cmd));

	/* this should fail, invalid command state */
	check_fail(ct_send, (cmd));
	if (ct_last_message.type != CTMSG_CLIENT2 || ct_last_message.number != 0x0101019b
	    || !strstr(ct_last_message.text, "idle"))
		report_wrong_error();
}
