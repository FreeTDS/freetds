/*
 * Test bulk copy in using blk_textxfer.
 * Text/image columns bound with a NULL buffer are sent separately
 * using blk_textxfer, possibly in multiple chunks.
 * Expected behaviour is taken from Sybase/SAP Open Client.
 */

#include "common.h"

#include <bkpublic.h>

static const char table_name[] = "blk_textxfer_test";

#define MAX_BLOB 100000

static CS_CONNECTION *conn;
static CS_COMMAND *cmd;
static CS_BLKDESC *blkdesc;

/* bound variables */
static CS_INT row_id, row_id_len = CS_UNUSED;
static CS_INT text_len, image_len, var_len;
static CS_SMALLINT text_ind, image_ind;
static char var_data[11];
static CS_BYTE text_data[MAX_BLOB];

static CS_BYTE blob_buf[MAX_BLOB];

/* expected table content, filled while sending data */
typedef struct
{
	int id;
	int text_len;		/* -1 for NULL */
	int image_len;		/* -1 for NULL */
} expected_row;

static expected_row expected[16];
static int num_expected;

/* message numbers, layer 1 (blk), origin 1 (user), severity 1 */
#define BLK_MSG(n) (0x01010100 | (n))
#define BLK_STATE_ERROR BLK_MSG(1)
#define BLK_TOO_MUCH_DATA BLK_MSG(7)
#define BLK_TOO_MANY_TEXTXFER BLK_MSG(49)

/* generate data for a given row and column */
static void
fill_blob(CS_BYTE *buf, int len, int id, int col)
{
	int i;

	for (i = 0; i < len; i++)
		buf[i] = (CS_BYTE) ('a' + (i * 7 + id * 3 + col) % 26);
}

static void
add_expected(int id, int text_len, int image_len)
{
	assert(num_expected < (int) (sizeof(expected) / sizeof(expected[0])));
	expected[num_expected].id = id;
	expected[num_expected].text_len = text_len;
	expected[num_expected].image_len = image_len;
	++num_expected;
}

/*
 * Start a bulk copy.
 * Bind text column with a NULL buffer if text_deferred is set,
 * image column is always bound with a NULL buffer.
 */
static void
start_in(bool text_deferred)
{
	CS_DATAFMT fmt;

	/*
	 * Use a new descriptor for each test, SAP library does not
	 * handle NULL values correctly reusing a descriptor.
	 */
	if (blkdesc)
		blk_drop(blkdesc);
	check_call(blk_alloc, (conn, BLK_VERSION_100, &blkdesc));

	check_call(blk_init, (blkdesc, CS_BLK_IN, (char *) table_name, CS_NULLTERM));

	memset(&fmt, 0, sizeof(fmt));
	fmt.count = 1;
	fmt.datatype = CS_INT_TYPE;
	fmt.maxlength = sizeof(row_id);
	check_call(blk_bind, (blkdesc, 1, &fmt, &row_id, &row_id_len, NULL));

	memset(&fmt, 0, sizeof(fmt));
	fmt.count = 1;
	fmt.datatype = CS_TEXT_TYPE;
	fmt.maxlength = MAX_BLOB;
	check_call(blk_bind, (blkdesc, 2, &fmt, text_deferred ? NULL : text_data, &text_len, &text_ind));

	memset(&fmt, 0, sizeof(fmt));
	fmt.count = 1;
	fmt.datatype = CS_IMAGE_TYPE;
	fmt.maxlength = MAX_BLOB;
	check_call(blk_bind, (blkdesc, 3, &fmt, NULL, &image_len, &image_ind));

	memset(&fmt, 0, sizeof(fmt));
	fmt.count = 1;
	fmt.datatype = CS_CHAR_TYPE;
	fmt.maxlength = sizeof(var_data) - 1;
	check_call(blk_bind, (blkdesc, 4, &fmt, var_data, &var_len, NULL));
}

/* set bound variables for a row, length -1 means NULL */
static void
set_row(int id, int tlen, int ilen)
{
	row_id = id;
	text_len = tlen < 0 ? 0 : tlen;
	text_ind = tlen < 0 ? -1 : 0;
	image_len = ilen < 0 ? 0 : ilen;
	image_ind = ilen < 0 ? -1 : 0;
	var_len = sprintf(var_data, "v%d", id);
	if (tlen > 0)
		fill_blob(text_data, tlen, id, 2);
}

static void
text_xfer(const CS_BYTE *buf, int len, CS_RETCODE expected_ret)
{
	CS_RETCODE ret;
	CS_INT outlen = 0;

	ret = blk_textxfer(blkdesc, (CS_BYTE *) buf, len, &outlen);
	if (ret != expected_ret) {
		fprintf(stderr, "blk_textxfer(%d) returned %d (%s), expected %d (%s)\n",
			len, (int) ret, res_type_str(ret), (int) expected_ret, res_type_str(expected_ret));
		exit(1);
	}
}

/* send a column using blk_textxfer in chunks of given size */
static void
send_blob(int id, int col, int len, int chunk)
{
	int pos;

	fill_blob(blob_buf, len, id, col);
	for (pos = 0; pos < len; pos += chunk) {
		int n = len - pos < chunk ? len - pos : chunk;

		text_xfer(blob_buf + pos, n, pos + n >= len ? CS_END_DATA : CS_SUCCEED);
	}
}

static void
done(CS_INT type, CS_INT expected_rows)
{
	CS_INT rows = -1;

	check_call(blk_done, (blkdesc, type, &rows));
	if (rows != expected_rows) {
		fprintf(stderr, "blk_done(%d) returned %d rows, expected %d\n", (int) type, (int) rows, (int) expected_rows);
		exit(1);
	}
}

static void
check_blob(int id, int col, const CS_BYTE *data, CS_INT len, CS_SMALLINT ind, int expected_len)
{
	if (expected_len < 0) {
		if (ind != -1) {
			fprintf(stderr, "row %d column %d: expected NULL\n", id, col);
			exit(1);
		}
		return;
	}
	if (ind == -1 || len != expected_len) {
		fprintf(stderr, "row %d column %d: got length %d (ind %d), expected %d\n", id, col, (int) len, (int) ind,
			expected_len);
		exit(1);
	}
	fill_blob(blob_buf, expected_len, id, col);
	if (memcmp(data, blob_buf, expected_len) != 0) {
		fprintf(stderr, "row %d column %d: wrong data\n", id, col);
		exit(1);
	}
}

/* check table content against expected rows, also checks connection is usable */
static void
check_table(void)
{
	char sql[128];
	CS_DATAFMT fmt;
	CS_INT result_type, count, id, tlen, ilen, vlen;
	CS_SMALLINT tind, iind, vind;
	static CS_BYTE tdata[MAX_BLOB], idata[MAX_BLOB];
	char vdata[11];
	CS_RETCODE ret;
	int num_rows = 0;

	sprintf(sql, "select id, t, i, v from %s order by id", table_name);
	check_call(ct_command, (cmd, CS_LANG_CMD, sql, CS_NULLTERM, CS_UNUSED));
	check_call(ct_send, (cmd));
	while ((ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch (result_type) {
		case CS_ROW_RESULT:
			memset(&fmt, 0, sizeof(fmt));
			fmt.count = 1;
			fmt.datatype = CS_INT_TYPE;
			fmt.maxlength = sizeof(id);
			check_call(ct_bind, (cmd, 1, &fmt, &id, NULL, NULL));
			fmt.datatype = CS_TEXT_TYPE;
			fmt.maxlength = MAX_BLOB;
			check_call(ct_bind, (cmd, 2, &fmt, tdata, &tlen, &tind));
			fmt.datatype = CS_IMAGE_TYPE;
			check_call(ct_bind, (cmd, 3, &fmt, idata, &ilen, &iind));
			fmt.datatype = CS_CHAR_TYPE;
			fmt.maxlength = sizeof(vdata) - 1;
			check_call(ct_bind, (cmd, 4, &fmt, vdata, &vlen, &vind));
			while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED) {
				const expected_row *row;
				char expected_var[16];

				if (num_rows >= num_expected) {
					fprintf(stderr, "too many rows in table\n");
					exit(1);
				}
				row = &expected[num_rows++];
				if (id != row->id) {
					fprintf(stderr, "got row id %d, expected %d\n", (int) id, row->id);
					exit(1);
				}
				check_blob(id, 2, tdata, tlen, tind, row->text_len);
				check_blob(id, 3, idata, ilen, iind, row->image_len);
				sprintf(expected_var, "v%d", (int) id);
				if (vind == -1 || vlen != (CS_INT) strlen(expected_var) || memcmp(vdata, expected_var, vlen) != 0) {
					fprintf(stderr, "row %d: wrong varchar column\n", (int) id);
					exit(1);
				}
			}
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
	if (num_rows != num_expected) {
		fprintf(stderr, "got %d rows, expected %d\n", num_rows, num_expected);
		exit(1);
	}
	printf("table content correct, %d rows\n", num_rows);
}

static void
clear_table(void)
{
	char sql[128];

	sprintf(sql, "delete from %s", table_name);
	check_call(run_command, (cmd, sql));
	num_expected = 0;
}

static void
test_chunks(void)
{
	printf("multiple rows and chunks\n");
	start_in(true);

	set_row(1, 2500, 1500);
	check_call(blk_rowxfer, (blkdesc));
	send_blob(1, 2, 2500, 1000);
	send_blob(1, 3, 1500, 1000);
	add_expected(1, 2500, 1500);

	/* large data, more packets needed */
	set_row(2, 70000, 40000);
	check_call(blk_rowxfer, (blkdesc));
	send_blob(2, 2, 70000, 7000);
	send_blob(2, 3, 40000, 4096);
	add_expected(2, 70000, 40000);
	done(CS_BLK_BATCH, 2);

	/* single chunk */
	set_row(3, 10, 20);
	check_call(blk_rowxfer, (blkdesc));
	send_blob(3, 2, 10, 10);
	send_blob(3, 3, 20, 20);
	add_expected(3, 10, 20);
	done(CS_BLK_ALL, 1);

	check_table();
	clear_table();
}

static void
test_nulls(void)
{
	printf("NULL values\n");
	start_in(true);

	/* NULL values do not require blk_textxfer */
	set_row(1, -1, 10);
	check_call(blk_rowxfer, (blkdesc));
	send_blob(1, 3, 10, 10);
	add_expected(1, -1, 10);

	set_row(2, -1, -1);
	check_call(blk_rowxfer, (blkdesc));
	add_expected(2, -1, -1);

	set_row(3, 5, -1);
	check_call(blk_rowxfer, (blkdesc));
	send_blob(3, 2, 5, 5);
	/* no more columns to send */
	check_fail(blk_textxfer, (blkdesc, blob_buf, 5, NULL));
	check_last_message(CTMSG_CLIENT, BLK_TOO_MANY_TEXTXFER, NULL);
	add_expected(3, 5, -1);

	done(CS_BLK_ALL, 3);

	check_table();
	clear_table();
}

static void
test_mixed(void)
{
	printf("text bound, image using blk_textxfer\n");
	start_in(false);

	set_row(1, 3000, 2000);
	check_call(blk_rowxfer, (blkdesc));
	send_blob(1, 3, 2000, 900);
	add_expected(1, 3000, 2000);

	set_row(2, 100, -1);
	check_call(blk_rowxfer, (blkdesc));
	add_expected(2, 100, -1);

	done(CS_BLK_ALL, 2);

	check_table();
	clear_table();
}

static void
test_errors(void)
{
	printf("wrong calls\n");
	start_in(true);

	/* no row sent yet */
	check_fail(blk_textxfer, (blkdesc, blob_buf, 10, NULL));
	check_last_message(CTMSG_CLIENT, BLK_STATE_ERROR, NULL);

	set_row(1, 10, 20);
	check_call(blk_rowxfer, (blkdesc));

	/* more data than declared, nothing is sent */
	check_fail(blk_textxfer, (blkdesc, blob_buf, 15, NULL));
	check_last_message(CTMSG_CLIENT, BLK_TOO_MUCH_DATA, NULL);
	send_blob(1, 2, 10, 4);
	add_expected(1, 10, 20);

	/* still data to send for the row */
	fill_blob(blob_buf, 20, 1, 3);
	text_xfer(blob_buf, 5, CS_SUCCEED);
	check_fail(blk_rowxfer, (blkdesc));
	check_last_message(CTMSG_CLIENT, BLK_STATE_ERROR, NULL);
	check_fail(blk_done, (blkdesc, CS_BLK_ALL, NULL));
	check_last_message(CTMSG_CLIENT, BLK_STATE_ERROR, NULL);

	/* cancel discards everything */
	check_call(blk_done, (blkdesc, CS_BLK_CANCEL, NULL));
	num_expected = 0;
	check_table();
}

static void
run_tests(const char *create_sql)
{
	char sql[512];

	sprintf(sql, "if exists (select 1 from sysobjects where type = 'U' and name = '%s') drop table %s", table_name, table_name);
	check_call(run_command, (cmd, sql));

	sprintf(sql, create_sql, table_name);
	if (run_command(cmd, sql) != CS_SUCCEED) {
		printf("table not supported, skipping\n");
		return;
	}
	printf("testing with %s\n", sql);

	test_chunks();
	test_nulls();
	test_mixed();
	test_errors();

	sprintf(sql, "drop table %s", table_name);
	check_call(run_command, (cmd, sql));
}

TEST_MAIN()
{
	CS_CONTEXT *ctx;
	bool verbose = false;

	printf("%s: Testing blk_textxfer\n", __FILE__);
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(run_command, (cmd, "set textsize 1000000"));

	run_tests("create table %s (id int not null, t text null, i image null, v varchar(10) null)");
	/* MSSQL 2005+, sent as PLP using TDS 7.2+ */
	run_tests("create table %s (id int not null, t varchar(max) null, i varbinary(max) null, v varchar(10) null)");

	if (blkdesc)
		blk_drop(blkdesc);

	printf("done\n");

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
