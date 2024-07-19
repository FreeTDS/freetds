#include "common.h"

/* Testing: Retrieving SQL_VARIANT */
TEST_MAIN()
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int verbose = 0;

	CS_RETCODE ret;
	CS_RETCODE results_ret;
	CS_INT result_type;
	CS_INT num_cols;

	CS_DATAFMT datafmt;
	CS_INT datalength;
	CS_SMALLINT ind;
	CS_INT count, row_count = 0;

	CS_CHAR select[1024];

	CS_CHAR col1[128];
	const char *expected[10];
	unsigned num_expected = 0;

	unsigned rows = 0;

	memset(expected, 0, sizeof(expected));

	printf("%s: Retrieve SQL_VARIANT column\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	strcpy(select, "CREATE TABLE #ctlib0009 (n int, col1 sql_variant null)");

	check_call(ct_command, (cmd, CS_LANG_CMD, select, CS_NULLTERM, CS_UNUSED));

	check_call(ct_send, (cmd));

	check_call(ct_results, (cmd, &result_type));

	switch (result_type) {
	case CS_CMD_FAIL:
		fprintf(stderr, "ct_results() result_type CS_CMD_FAIL, probably not MSSQL.\n");
		try_ctlogout(ctx, conn, cmd, verbose);
		return 0;
	case CS_CMD_SUCCEED:
		break;
	default:
		fprintf(stderr, "ct_results() unexpected return %d.\n", result_type);
		try_ctlogout(ctx, conn, cmd, verbose);
		return 1;
	}

	check_call(run_command, (cmd, "insert into #ctlib0009 values (1, 123)"));
	expected[num_expected++] = "123";

	check_call(run_command, (cmd, "insert into #ctlib0009 values (2, NULL)"));
	expected[num_expected++] = "";

	check_call(run_command, (cmd, "insert into #ctlib0009 values (3, 'hello')"));
	expected[num_expected++] = "hello";

	check_call(run_command, (cmd, "insert into #ctlib0009 values (4, 123.456)"));
	expected[num_expected++] = "123.456";

	strcpy(select, "select col1 from #ctlib0009 order by n");

	check_call(ct_command, (cmd, CS_LANG_CMD, select, CS_NULLTERM, CS_UNUSED));

	check_call(ct_send, (cmd));

	ct_callback(ctx, NULL, CS_SET, CS_SERVERMSG_CB, (CS_VOID *) servermsg_cb);
	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		printf("ct_results returned %s type\n", res_type_str(result_type));
		switch ((int) result_type) {
		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_CMD_FAIL:
			fprintf(stderr, "ct_results() result_type CS_CMD_FAIL.\n");
			return 1;
		case CS_ROW_RESULT:
			check_call(ct_res_info, (cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));
			if (num_cols != 1) {
				fprintf(stderr, "num_cols %d != 1", num_cols);
				return 1;
			}

			check_call(ct_describe, (cmd, 1, &datafmt));
			datafmt.format = CS_FMT_UNUSED;
			if (datafmt.maxlength > sizeof(col1)) {
				datafmt.maxlength = sizeof(col1);
			}
			check_call(ct_bind, (cmd, 1, &datafmt, col1, &datalength, &ind));

			while (((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				if (ret == CS_ROW_FAIL) {
					fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
					return 1;
				} else {	/* ret == CS_SUCCEED */
					col1[datalength] = 0;
					printf("col1 = %s\n", col1);
					assert(strcmp(col1, expected[rows]) == 0);
					++rows;
				}
			}


			switch ((int) ret) {
			case CS_END_DATA:
				break;
			case CS_FAIL:
				fprintf(stderr, "ct_fetch() returned CS_FAIL.\n");
				return 1;
			default:
				fprintf(stderr, "ct_fetch() unexpected return.\n");
				return 1;
			}
			break;

		default:
			fprintf(stderr, "ct_results() unexpected result_type.\n");
			return 1;
		}
	}
	switch ((int) results_ret) {
	case CS_END_RESULTS:
		break;
	case CS_FAIL:
		fprintf(stderr, "ct_results() failed.\n");
		return 1;
		break;
	default:
		fprintf(stderr, "ct_results() unexpected return.\n");
		return 1;
	}

	if (rows != 4) {
		fprintf(stderr, "wrong number of rows: normal %u, expected 4\n", rows);
		return 1;
	}

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
