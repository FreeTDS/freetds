/* try login and logout multiple times in a row */

#include <config.h>

#include <stdio.h>
#include <ctpublic.h>
#include "common.h"

int
main(void)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int verbose = 0;
	int i;

	printf("%s: Testing login, logout\n", __FILE__);

	for (i =0; i < 100; ++i) {

		if (verbose)
			printf("Trying login\n");

		check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

		if (verbose)
			printf("Trying logout\n");

		check_call(try_ctlogout, (ctx, conn, cmd, verbose));
	}

	if (verbose)
		printf("Test succeeded\n");

	return 0;
}
