#include "common.h"

#include <cspublic.h>

/*
 * ct_send SQL |select name = @@servername|
 * ct_bind variable
 * ct_fetch and print results
 */
TEST_MAIN()
{
	int verbose = 1;
	CS_CONTEXT *ctx;
	CS_RETCODE ret;
	CS_DATAFMT srcfmt;
	CS_INT src = 32768;
	CS_DATAFMT dstfmt;
	CS_SMALLINT dst;
	CS_DATETIME dst_date;

	int i;

	CS_INT num_msgs;
	CS_CLIENTMSG client_message;

	printf("%s: Testing context callbacks\n", __FILE__);

	srcfmt.datatype = CS_INT_TYPE;
	srcfmt.maxlength = sizeof(CS_INT);
	srcfmt.locale = NULL;

	dstfmt.datatype = CS_SMALLINT_TYPE;
	dstfmt.maxlength = sizeof(CS_SMALLINT);
	dstfmt.locale = NULL;

	if (verbose) {
		printf("Trying clientmsg_cb with context\n");
	}
	check_call(cs_ctx_alloc, (CS_VERSION_100, &ctx));
	check_call(ct_init, (ctx, CS_VERSION_100));

	check_call(cs_diag, (ctx, CS_INIT, CS_UNUSED, CS_UNUSED, NULL));

	if (cs_convert(ctx, &srcfmt, &src, &dstfmt, &dst, NULL) == CS_SUCCEED) {
		fprintf(stderr, "cs_convert() succeeded when failure was expected\n");
		return 1;
	}

	dstfmt.datatype = CS_DATETIME_TYPE;
	dstfmt.maxlength = sizeof(CS_DATETIME);
	dstfmt.locale = NULL;

	if (cs_convert(ctx, &srcfmt, &src, &dstfmt, &dst_date, NULL) == CS_SUCCEED) {
		fprintf(stderr, "cs_convert() succeeded when failure was expected\n");
		return 1;
	}

	check_call(cs_diag, (ctx, CS_STATUS, CS_CLIENTMSG_TYPE, CS_UNUSED, &num_msgs));

	for (i = 0; i < num_msgs; i++ ) {
		check_call(cs_diag, (ctx, CS_GET, CS_CLIENTMSG_TYPE, i + 1, &client_message));
	
		cslibmsg_cb(NULL, &client_message);
	}

	if ((ret = cs_diag(ctx, CS_GET, CS_CLIENTMSG_TYPE, i + 1, &client_message)) != CS_NOMSG) {
		fprintf(stderr, "cs_diag(CS_GET) did not fail with CS_NOMSG\n");
		return 1;
	}

	check_call(cs_diag, (ctx, CS_CLEAR, CS_CLIENTMSG_TYPE, CS_UNUSED, NULL));

	check_call(cs_diag, (ctx, CS_STATUS, CS_CLIENTMSG_TYPE, CS_UNUSED, &num_msgs));
	if (num_msgs != 0) {
		fprintf(stderr, "cs_diag(CS_CLEAR) failed there are still %d messages on queue\n", num_msgs);
		return 1;
	}

	check_call(ct_exit, (ctx, CS_UNUSED));
	check_call(cs_ctx_drop, (ctx));

	return 0;
}
