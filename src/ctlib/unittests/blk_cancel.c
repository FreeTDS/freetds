/*
 * Test bulk copy cancellation (blk_done with CS_BLK_CANCEL).
 * Check data not committed are discarded and that connection
 * can be used after cancellation.
 */

#include "common.h"

#include <bkpublic.h>

static const char table_name[] = "blk_cancel_test";

static CS_CONNECTION *conn;
static CS_COMMAND *cmd;
static CS_BLKDESC *blkdesc;

static CS_INT row_id, row_id_len = sizeof(row_id);
static char row_data[201];
static CS_INT row_data_len;

/* start a bulk copy into the table binding the columns */
static void
start_in(void)
{
	CS_DATAFMT fmt;

	check_call(blk_init, (blkdesc, CS_BLK_IN, (char *) table_name, CS_NULLTERM));

	memset(&fmt, 0, sizeof(fmt));
	fmt.count = 1;
	fmt.datatype = CS_INT_TYPE;
	fmt.maxlength = sizeof(row_id);
	check_call(blk_bind, (blkdesc, 1, &fmt, &row_id, &row_id_len, NULL));

	memset(&fmt, 0, sizeof(fmt));
	fmt.count = 1;
	fmt.datatype = CS_CHAR_TYPE;
	fmt.maxlength = sizeof(row_data) - 1;
	check_call(blk_bind, (blkdesc, 2, &fmt, row_data, &row_data_len, NULL));
}

/* send some rows, enough rows cause packets to be sent to the server */
static void
send_rows(int first, int num)
{
	int i;

	for (i = 0; i < num; i++) {
		row_id = first + i;
		row_data_len = 50 + row_id % 150;
		memset(row_data, 'a' + row_id % 26, row_data_len);
		check_call(blk_rowxfer, (blkdesc));
	}
}

static void
done(CS_INT type, CS_INT expected)
{
	CS_INT rows = -1;

	check_call(blk_done, (blkdesc, type, &rows));
	if (rows != expected) {
		fprintf(stderr, "blk_done(%d) returned %d rows, expected %d\n", (int) type, (int) rows, (int) expected);
		exit(1);
	}
}

/* check table content, this also check the connection is still usable */
static void
check_table(CS_INT expected_count, CS_INT expected_sum)
{
	char sql[128];
	CS_DATAFMT fmt;
	CS_INT values[2], result_type, count, num_rows = 0;
	CS_RETCODE ret;
	int i;

	sprintf(sql, "select count(*), coalesce(sum(id), 0) from %s", table_name);
	check_call(ct_command, (cmd, CS_LANG_CMD, sql, CS_NULLTERM, CS_UNUSED));
	check_call(ct_send, (cmd));
	while ((ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch (result_type) {
		case CS_ROW_RESULT:
			for (i = 0; i < 2; i++) {
				memset(&fmt, 0, sizeof(fmt));
				fmt.datatype = CS_INT_TYPE;
				fmt.maxlength = sizeof(CS_INT);
				fmt.count = 1;
				check_call(ct_bind, (cmd, i + 1, &fmt, &values[i], NULL, NULL));
			}
			while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
				++num_rows;
			assert(ret == CS_END_DATA);
			break;
		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;
		default:
			fprintf(stderr, "unexpected result type %d\n", (int) result_type);
			exit(1);
		}
	}
	assert(ret == CS_END_RESULTS);
	assert(num_rows == 1);

	printf("table has %d rows, sum %d\n", (int) values[0], (int) values[1]);
	if (values[0] != expected_count || values[1] != expected_sum) {
		fprintf(stderr, "wrong table content, expected %d rows, sum %d\n", (int) expected_count, (int) expected_sum);
		exit(1);
	}
}

static void
clear_table(void)
{
	char sql[128];

	sprintf(sql, "delete from %s", table_name);
	check_call(run_command, (cmd, sql));
}

/* sum of ids from first to first + num - 1 */
static CS_INT
sum_ids(int first, int num)
{
	return (CS_INT) (((long) first * 2 + num - 1) * num / 2);
}

TEST_MAIN()
{
	CS_CONTEXT *ctx;
	bool verbose = false;
	char sql[256];
	CS_DATAFMT fmt;
	CS_INT id, id_len, rows;
	int i;

	printf("%s: Testing bulk copy cancellation\n", __FILE__);
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	sprintf(sql, "if exists (select 1 from sysobjects where type = 'U' and name = '%s') drop table %s", table_name, table_name);
	check_call(run_command, (cmd, sql));

	sprintf(sql, "create table %s (id int not null, data varchar(200) not null)", table_name);
	check_call(run_command, (cmd, sql));

	check_call(blk_alloc, (conn, BLK_VERSION_100, &blkdesc));

	printf("cancel with data not sent yet to server\n");
	start_in();
	send_rows(1, 5);
	done(CS_BLK_CANCEL, 0);
	check_table(0, 0);

	printf("cancel after some packets were sent to server\n");
	start_in();
	send_rows(1, 2000);
	done(CS_BLK_CANCEL, 0);
	check_table(0, 0);

	printf("cancel after a committed batch\n");
	start_in();
	send_rows(1, 10);
	done(CS_BLK_BATCH, 10);
	send_rows(11, 2000);
	done(CS_BLK_CANCEL, 0);
	check_table(10, sum_ids(1, 10));
	clear_table();

	printf("cancel just after a batch\n");
	start_in();
	send_rows(1, 10);
	done(CS_BLK_BATCH, 10);
	done(CS_BLK_CANCEL, 0);
	check_table(10, sum_ids(1, 10));
	clear_table();

	printf("cancel before transferring any row\n");
	start_in();
	done(CS_BLK_CANCEL, 0);
	check_table(0, 0);

	printf("cancel twice, second does nothing\n");
	start_in();
	send_rows(1, 5);
	done(CS_BLK_CANCEL, 0);
	done(CS_BLK_CANCEL, 0);
	check_table(0, 0);

	printf("normal copy after cancellation\n");
	start_in();
	send_rows(1, 2000);
	done(CS_BLK_ALL, 2000);
	check_table(2000, sum_ids(1, 2000));

	printf("cancel copy out\n");
	check_call(blk_init, (blkdesc, CS_BLK_OUT, (char *) table_name, CS_NULLTERM));
	memset(&fmt, 0, sizeof(fmt));
	fmt.count = 1;
	fmt.datatype = CS_INT_TYPE;
	fmt.maxlength = sizeof(id);
	check_call(blk_bind, (blkdesc, 1, &fmt, &id, &id_len, NULL));
	for (i = 0; i < 5; i++) {
		check_call(blk_rowxfer_mult, (blkdesc, &rows));
		assert(rows == 1);
	}
	done(CS_BLK_CANCEL, 0);
	check_table(2000, sum_ids(1, 2000));

	blk_drop(blkdesc);

	sprintf(sql, "drop table %s", table_name);
	check_call(run_command, (cmd, sql));

	printf("done\n");

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
