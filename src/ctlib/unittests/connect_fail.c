#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <ctpublic.h>
#include "common.h"

static char software_version[] = "$Id: connect_fail.c,v 1.1 2003-01-12 10:05:04 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char **argv)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_RETCODE ret;

	read_login_info();
	
	if (cs_ctx_alloc(CS_VERSION_100, &ctx) != CS_SUCCEED) {
		fprintf(stderr, "Context Alloc failed!\n");
		return ret;
	}
	if (ct_init(ctx, CS_VERSION_100) != CS_SUCCEED) {
		fprintf(stderr, "Library Init failed!\n");
		return ret;
	}
	if (ct_con_alloc(ctx, &conn) != CS_SUCCEED) {
		fprintf(stderr, "Connect Alloc failed!\n");
		return ret;
	}
	if (ct_con_props(conn, CS_SET, CS_USERNAME, "sa", CS_NULLTERM, NULL) != CS_SUCCEED) {
		fprintf(stderr, "ct_con_props() SET USERNAME failed!\n");
		return ret;
	}
	if (ct_con_props(conn, CS_SET, CS_PASSWORD, "invalid", CS_NULLTERM, NULL) != CS_SUCCEED) {
		fprintf(stderr, "ct_con_props() SET PASSWORD failed!\n");
		return ret;
	}
	if (ct_connect(conn, SERVER, CS_NULLTERM) != CS_FAIL) {
		fprintf(stderr, "Connection succeeded??\n");
		return ret;
	}

	if (ct_cancel(conn, NULL, CS_CANCEL_ALL) != CS_SUCCEED) {
		fprintf(stderr, "ct_cancel() failed!\n");
		return ret;
	}
	if (ct_close(conn, CS_UNUSED) != CS_SUCCEED) {
		fprintf(stderr, "ct_close() failed!\n");
		return ret;
	}
	if (ct_con_drop(conn) != CS_SUCCEED) {
		fprintf(stderr, "ct_con_drop() failed!\n");
		return ret;
	}
	if (ct_exit(ctx, CS_UNUSED) != CS_SUCCEED) {
		fprintf(stderr, "ct_exit() failed!\n");
		return ret;
	}
	if (cs_ctx_drop(ctx) != CS_SUCCEED) {
		fprintf(stderr, "cs_ctx_drop() failed!\n");
		return ret;
	}

	fprintf(stdout, "Test suceeded\n");
	return 0;
}
