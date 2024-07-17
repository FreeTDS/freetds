#include "common.h"

TEST_MAIN()
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	int ret = 1;

	read_login_info();

	check_call(cs_ctx_alloc, (CS_VERSION_100, &ctx));
	check_call(ct_init, (ctx, CS_VERSION_100));
	check_call(ct_con_alloc, (ctx, &conn));
	check_call(ct_con_props, (conn, CS_SET, CS_USERNAME, (CS_VOID*) "sa", CS_NULLTERM, NULL));
	check_call(ct_con_props, (conn, CS_SET, CS_PASSWORD, (CS_VOID*) "invalid", CS_NULLTERM, NULL));
	if (ct_connect(conn, common_pwd.server, CS_NULLTERM) != CS_FAIL) {
		fprintf(stderr, "Connection succeeded??\n");
		return ret;
	}

	check_call(ct_cancel, (conn, NULL, CS_CANCEL_ALL));
	check_call(ct_close, (conn, CS_UNUSED));
	check_call(ct_con_drop, (conn));
	check_call(ct_exit, (ctx, CS_UNUSED));
	check_call(cs_ctx_drop,(ctx));

	printf("Test succeeded\n");
	return 0;
}
