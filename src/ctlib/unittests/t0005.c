/* try login and logout multiple times in a row */

#include <config.h>

#include <stdio.h>
#include <ctpublic.h>
#include "common.h"

int
main(int argc, char **argv)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	CS_RETCODE ret;
	int verbose = 0;
	int i;

	printf("%s: Testing login, logout\n", __FILE__);

	for (i =0; i < 100; ++i) {

		if (verbose)
			printf("Trying login\n");

		ret = try_ctlogin(&ctx, &conn, &cmd, verbose);
		if (ret != CS_SUCCEED) {
			fprintf(stderr, "Login failed\n");
			return 1;
		}

		if (verbose)
			printf("Trying logout\n");

		ret = try_ctlogout(ctx, conn, cmd, verbose);
		if (ret != CS_SUCCEED) {
			fprintf(stderr, "Logout failed\n");
			return 2;
		}
	}

	if (verbose)
		printf("Test succeeded\n");

	return 0;
}
