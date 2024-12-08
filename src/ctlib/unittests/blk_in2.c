/*
 * Test from MATSUMOTO, Tadashi
 * Cfr "blk_init fails by the even number times execution" on ML, 2007-03-09
 * This mix bulk and cancel
 */

#include "common.h"

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

#include <bkpublic.h>

static const char create_table_sql[] = "CREATE TABLE hogexxx (col varchar(100))";

static void
hoge_blkin(CS_CONNECTION * con, CS_BLKDESC * blk, char *table, char *data)
{
	CS_DATAFMT meta = { "" };
	CS_INT length = 5;
	CS_INT row = 0;

	check_call(ct_cancel, (con, NULL, CS_CANCEL_ALL));
	check_call(blk_init, (blk, CS_BLK_IN, table, CS_NULLTERM));

	check_call(blk_props, (blk, CS_SET, BLK_HINTS, "TABLOCK", CS_NULLTERM, NULL));

	meta.count = 1;
	meta.datatype = CS_CHAR_TYPE;
	meta.format = CS_FMT_PADBLANK;
	meta.maxlength = 5;

	check_call(blk_bind, (blk, (int) 1, &meta, data, &length, NULL));
	check_call(blk_rowxfer, (blk));
	check_call(blk_done, (blk, CS_BLK_ALL, &row));
}

int
main(void)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	CS_BLKDESC *blkdesc;
	int verbose = 0;
	int i;
	char command[512];


	static char table_name[20] = "hogexxx";

	printf("%s: Inserting data using bulk cancelling\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	sprintf(command, "if exists (select 1 from sysobjects where type = 'U' and name = '%s') drop table %s",
		table_name, table_name);

	check_call(run_command, (cmd, command));

	check_call(run_command, (cmd, create_table_sql));

	check_call(blk_alloc, (conn, BLK_VERSION_100, &blkdesc));

	for (i = 0; i < 10; i++) {
		/* compute some data */
		memset(command, ' ', sizeof(command));
		memset(command, 'a' + i, (i * 37) % 11);

		hoge_blkin(conn, blkdesc, table_name, command);
	}

	blk_drop(blkdesc);

	/* TODO test correct insert */

	printf("done\n");

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}

