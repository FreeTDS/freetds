#include "common.h"

#include <bkpublic.h>

/* Testing: array binding of result set */
int
main(void)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	CS_BLKDESC *blkdesc;
	int verbose = 0;

	CS_RETCODE ret;

	CS_DATAFMT datafmt;
	CS_INT count = 0;

	CS_INT  col1[2];
	CS_CHAR col2[2][5];
	CS_CHAR col3[2][32];
	CS_INT      lencol1[2];
	CS_SMALLINT indcol1[2];
	CS_INT      lencol2[2];
	CS_SMALLINT indcol2[2];
	CS_INT      lencol3[2];
	CS_SMALLINT indcol3[2];

	int i;


	printf("%s: Retrieve data using array binding \n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	/* do not test error */
	ret = run_command(cmd, "IF OBJECT_ID('tempdb..#ctlibarray') IS NOT NULL DROP TABLE #ctlibarray");

	check_call(run_command, (cmd, "CREATE TABLE #ctlibarray (col1 int null,  col2 char(4) not null, col3 datetime not null)"));
	check_call(run_command, (cmd, "insert into #ctlibarray values (1, 'AAAA', 'Jan  1 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlibarray values (2, 'BBBB', 'Jan  2 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlibarray values (3, 'CCCC', 'Jan  3 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlibarray values (8, 'DDDD', 'Jan  4 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlibarray values (NULL, 'EEEE', 'Jan  5 2002 10:00:00AM')"));

	check_call(blk_alloc, (conn, BLK_VERSION_100, &blkdesc));

	check_call(blk_init, (blkdesc, CS_BLK_OUT, "#ctlibarray", CS_NULLTERM));

	check_call(blk_describe, (blkdesc, 1, &datafmt));

	datafmt.format = CS_FMT_UNUSED;
	if (datafmt.maxlength > 1024) {
		datafmt.maxlength = 1024;
	}

	datafmt.count = 2;

	check_call(blk_bind, (blkdesc, 1, &datafmt, &col1[0], &lencol1[0], &indcol1[0]));

	check_call(blk_describe, (blkdesc, 2, &datafmt));

	datafmt.format = CS_FMT_NULLTERM;
	datafmt.maxlength = 5;
	datafmt.count = 2;

	check_call(blk_bind, (blkdesc, 2, &datafmt, col2[0], &lencol2[0], &indcol2[0]));

	check_call(blk_describe, (blkdesc, 3, &datafmt));

	datafmt.datatype = CS_CHAR_TYPE;
	datafmt.format = CS_FMT_NULLTERM;
	datafmt.maxlength = 32;
	datafmt.count = 2;

	check_call(blk_bind, (blkdesc, 3, &datafmt, col3[0], &lencol3[0], &indcol3[0]));

	while((ret = blk_rowxfer_mult(blkdesc, &count)) == CS_SUCCEED) {
		for(i = 0; i < count; i++) {
			printf("retrieved %d (%d,%d) %s (%d,%d) %s (%d,%d)\n", 
				col1[i], lencol1[i], indcol1[i],
				col2[i], lencol2[i], indcol2[i],
				col3[i], lencol3[i], indcol3[i] );
		}
	}
	switch (ret) {
	case CS_END_DATA:
		for(i = 0; i < count; i++) {
			printf("retrieved %d (%d,%d) %s (%d,%d) %s (%d,%d)\n", 
				col1[i], lencol1[i], indcol1[i],
				col2[i], lencol2[i], indcol2[i],
				col3[i], lencol3[i], indcol3[i] );
		}
		break;
	case CS_FAIL:
	case CS_ROW_FAIL:
		fprintf(stderr, "blk_rowxfer_mult() failed\n");
		return 1;
	}

	blk_drop(blkdesc);

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
