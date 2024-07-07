#include "common.h"

#include <cspublic.h>

/*
 * ct_send SQL |select name = @@servername|
 * ct_bind variable
 * ct_fetch and print results
 */
int
main(void)
{
	int verbose = 1;
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	CS_DATAFMT srcfmt;
	CS_INT src = 32768;
	CS_DATAFMT dstfmt;
	CS_SMALLINT dst;

	printf("%s: Testing context callbacks\n", __FILE__);
	srcfmt.datatype = CS_INT_TYPE;
	srcfmt.maxlength = sizeof(CS_INT);
	srcfmt.locale = NULL;
#if 0
	dstfmt.datatype = CS_SMALLINT_TYPE;
#else
	dstfmt.datatype = CS_DATETIME_TYPE;
#endif
	dstfmt.maxlength = sizeof(CS_SMALLINT);
	dstfmt.locale = NULL;

	if (verbose) {
		printf("Trying clientmsg_cb with context\n");
	}
	check_call(cs_ctx_alloc, (CS_VERSION_100, &ctx));
	check_call(ct_init, (ctx, CS_VERSION_100));

	check_call(ct_callback, (ctx, NULL, CS_SET, CS_CLIENTMSG_CB, (CS_VOID*) clientmsg_cb));
	clientmsg_cb_invoked = 0;
	if (cs_convert(ctx, &srcfmt, &src, &dstfmt, &dst, NULL) == CS_SUCCEED) {
		fprintf(stderr, "cs_convert() succeeded when failure was expected\n");
		return 1;
	}
	if (clientmsg_cb_invoked != 0) {
		fprintf(stderr, "clientmsg_cb was invoked!\n");
		return 1;
	}

	if (verbose) {
		printf("Trying cslibmsg_cb\n");
	}
	check_call(cs_config, (ctx, CS_SET, CS_MESSAGE_CB, (CS_VOID*) cslibmsg_cb, CS_UNUSED, NULL));
	cslibmsg_cb_invoked = 0;
	if (cs_convert(ctx, &srcfmt, &src, &dstfmt, &dst, NULL) == CS_SUCCEED) {
		fprintf(stderr, "cs_convert() succeeded when failure was expected\n");
		return 1;
	}
	if (cslibmsg_cb_invoked == 0) {
		fprintf(stderr, "cslibmsg_cb was not invoked!\n");
		return 1;
	}

	check_call(ct_exit, (ctx, CS_UNUSED));
	check_call(cs_ctx_drop, (ctx));

	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	if (verbose) {
		printf("Trying clientmsg_cb with connection\n");
	}
	check_call(ct_callback, (NULL, conn, CS_SET, CS_CLIENTMSG_CB, (CS_VOID *) clientmsg_cb));
	clientmsg_cb_invoked = 0;
	check_call(run_command, (cmd, "."));
	if (clientmsg_cb_invoked) {
		fprintf(stderr, "clientmsg_cb was invoked!\n");
		return 1;
	}

	if (verbose) {
		printf("Trying servermsg_cb with connection\n");
	}
	check_call(ct_callback, (NULL, conn, CS_SET, CS_SERVERMSG_CB, (CS_VOID *) servermsg_cb));
	servermsg_cb_invoked = 0;
#if 0
	check_call(run_command, (cmd, "raiserror 99999 'This is a test'"));
	check_call(run_command, (cmd, "raiserror('This is a test', 17, 1)"));
#else
	check_call(run_command, (cmd, "print 'This is a test'"));
#endif
	if (servermsg_cb_invoked == 0) {
		fprintf(stderr, "servermsg_cb was not invoked!\n");
		return 1;
	}

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
