/*
 * Test bulk copy in using array binding and blk_rowxfer_mult.
 * Check the row count passed to blk_rowxfer_mult, behaviour taken
 * from Sybase/SAP library.
 */

#include "common.h"

#include <bkpublic.h>

static const char table_name[] = "blk_in_array_test";

static CS_CONNECTION *conn;
static CS_COMMAND *cmd;

/* check rows in table and their sum, also checks connection is usable */
static void
check_table(CS_INT expected_count, CS_INT expected_sum)
{
	char sql[128];
	CS_DATAFMT fmt;
	CS_INT values[2], result_type, count;
	CS_RETCODE ret;
	int i;

	sprintf(sql, "select count(*), coalesce(sum(id), 0) from %s", table_name);
	check_call(ct_command, (cmd, CS_LANG_CMD, sql, CS_NULLTERM, CS_UNUSED));
	check_call(ct_send, (cmd));
	while ((ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		if (result_type != CS_ROW_RESULT)
			continue;
		for (i = 0; i < 2; i++) {
			memset(&fmt, 0, sizeof(fmt));
			fmt.datatype = CS_INT_TYPE;
			fmt.maxlength = sizeof(CS_INT);
			fmt.count = 1;
			check_call(ct_bind, (cmd, i + 1, &fmt, &values[i], NULL, NULL));
		}
		while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			continue;
		assert(ret == CS_END_DATA);
	}
	assert(ret == CS_END_RESULTS);

	if (values[0] != expected_count || values[1] != expected_sum) {
		fprintf(stderr, "wrong table content, got %d rows sum %d, expected %d rows sum %d\n",
			(int) values[0], (int) values[1], (int) expected_count, (int) expected_sum);
		exit(1);
	}
}

/*
 * Bulk copy 3 bound rows (ids 1, 2 and 3) calling blk_rowxfer_mult
 * with given row count (NULL if row_count is NULL).
 */
static void
test(const CS_INT *row_count, CS_RETCODE expected_ret, CS_INT expected_rows)
{
	CS_BLKDESC *blkdesc;
	CS_DATAFMT fmt;
	CS_INT ids[3] = { 1, 2, 3 }, lens[3] = { 4, 4, 4 };
	CS_INT count = 0, rows = -1;
	CS_RETCODE ret;
	char sql[128];

	printf("row count %s%d\n", row_count ? "" : "NULL ", row_count ? (int) *row_count : 0);

	sprintf(sql, "delete from %s", table_name);
	check_call(run_command, (cmd, sql));

	check_call(blk_alloc, (conn, BLK_VERSION_100, &blkdesc));
	check_call(blk_init, (blkdesc, CS_BLK_IN, (char *) table_name, CS_NULLTERM));

	memset(&fmt, 0, sizeof(fmt));
	fmt.datatype = CS_INT_TYPE;
	fmt.maxlength = sizeof(CS_INT);
	fmt.count = 3;
	check_call(blk_bind, (blkdesc, 1, &fmt, ids, lens, NULL));

	if (row_count)
		count = *row_count;
	ct_reset_last_message();
	ret = blk_rowxfer_mult(blkdesc, row_count ? &count : NULL);
	if (ret != expected_ret) {
		fprintf(stderr, "blk_rowxfer_mult returned %d, expected %d\n", (int) ret, (int) expected_ret);
		exit(1);
	}
	if (expected_ret == CS_FAIL)
		check_last_message(CTMSG_CLIENT, 0x01010121, "row count greater than the array bind count");

	check_call(blk_done, (blkdesc, CS_BLK_ALL, &rows));
	if (rows != expected_rows) {
		fprintf(stderr, "blk_done returned %d rows, expected %d\n", (int) rows, (int) expected_rows);
		exit(1);
	}
	blk_drop(blkdesc);

	check_table(expected_rows, expected_rows * (expected_rows + 1) / 2);
}

TEST_MAIN()
{
	CS_CONTEXT *ctx;
	bool verbose = false;
	char sql[256];
	CS_INT row_count;

	printf("%s: Testing bulk copy in using array binding\n", __FILE__);
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	sprintf(sql, "if exists (select 1 from sysobjects where type = 'U' and name = '%s') drop table %s", table_name, table_name);
	check_call(run_command, (cmd, sql));
	sprintf(sql, "create table %s (id int not null)", table_name);
	check_call(run_command, (cmd, sql));

	/* NULL, zero or negative values send all bound rows */
	test(NULL, CS_SUCCEED, 3);
	row_count = 0;
	test(&row_count, CS_SUCCEED, 3);
	row_count = -1;
	test(&row_count, CS_SUCCEED, 3);

	/* positive values limit rows sent */
	row_count = 1;
	test(&row_count, CS_SUCCEED, 1);
	row_count = 2;
	test(&row_count, CS_SUCCEED, 2);
	row_count = 3;
	test(&row_count, CS_SUCCEED, 3);

	/* cannot send more rows than bound */
	row_count = 4;
	test(&row_count, CS_FAIL, 0);

	sprintf(sql, "drop table %s", table_name);
	check_call(run_command, (cmd, sql));

	printf("done\n");

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
