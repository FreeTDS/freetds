#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <stdio.h>
#include <ctpublic.h>
#include "common.h"

static char software_version[] = "$Id: ct_options.c,v 1.3 2004-09-08 12:51:24 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/* Testing: Set and get options with ct_options */
int
main(int argc, char *argv[])
{
	int verbose = 0;

	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	CS_RETCODE ret;

	CS_INT action;
	CS_INT option;
	CS_INT param;
	CS_INT paramlen;
	CS_INT outlen;

	/* this strange hack is used to check a bus error on some machines */
	CS_BOOL param_bools[2];

#define param_bool *(param_bools+1)

	if (verbose) {
		fprintf(stdout, "Trying login\n");
	}

	if (argc >= 5) {
		common_pwd.initialized = argc;
		strcpy(common_pwd.SERVER, argv[1]);
		strcpy(common_pwd.DATABASE, argv[2]);
		strcpy(common_pwd.USER, argv[3]);
		strcpy(common_pwd.PASSWORD, argv[4]);
	}

	ret = try_ctlogin(&ctx, &conn, &cmd, verbose);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "Login failed\n");
		return 1;
	}

	action = CS_GET;
	option = CS_TDS_VERSION;
	param = CS_TRUE;
	paramlen = sizeof(param);
	outlen = sizeof(param);

	ret = ct_con_props(conn, action, option, &param, paramlen, &outlen);
	if (param != CS_TDS_50) {
		fprintf(stdout, "%s: ct_options implemented only in TDS 5.0.\n", __FILE__);
		try_ctlogout(ctx, conn, cmd, verbose);
		return 0;
	}


	fprintf(stdout, "%s: Retrieve a boolean option\n", __FILE__);

	action = CS_GET;
	option = CS_OPT_CHAINXACTS;
	param_bool = CS_TRUE;
	paramlen = sizeof(param_bool);
	outlen = sizeof(param_bool);

	ret = ct_options(conn, action, option, &param_bool, paramlen, &outlen);

	if (ret != CS_SUCCEED) {
		ret = param_bool;
		fprintf(stdout, "%s:%d: CS_OPT_CHAINXACTS failed %d\n", __FILE__, __LINE__, ret);
		return 1;
	}

	ret = param_bool;
	fprintf(stdout, "%s:%d: CS_OPT_CHAINXACTS is %d\n", __FILE__, __LINE__, ret);

	if (verbose) {
		fprintf(stdout, "Trying logout\n");
	}
	ret = try_ctlogout(ctx, conn, cmd, verbose);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "Logout failed\n");
		return 1;
	}

	return 0;
}
