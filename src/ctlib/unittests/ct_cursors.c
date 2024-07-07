#include "common.h"

int
main(void)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	CS_COMMAND *cmd2;
	CS_COMMAND *cmd3;
	CS_RETCODE ret;
	CS_RETCODE ret2;
	CS_RETCODE results_ret;
	CS_INT result_type;
	CS_INT count, count2, row_count = 0;
	CS_DATAFMT datafmt;
	CS_DATAFMT datafmt2;
	CS_SMALLINT ind;
	int verbose = 1;
	CS_CHAR *name = "c1";
	CS_CHAR *name2 = "c2";
	CS_CHAR col1[6];
	CS_CHAR col2[6];
	CS_INT datalength;
	CS_CHAR text[128];
	CS_INT num_cols, i;

	memset(col1, 0, sizeof(col1));
	memset(col2, 0, sizeof(col2));

	printf("%s: use multiple cursors on the same connection\n", __FILE__);

	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(ct_cmd_alloc, (conn, &cmd2));

	check_call(ct_cmd_alloc, (conn, &cmd3));

	check_call(run_command, (cmd, "CREATE TABLE #test_table (col1 char(4))"));
	check_call(run_command, (cmd, "INSERT #test_table (col1) VALUES ('AAA')"));
	check_call(run_command, (cmd, "INSERT #test_table (col1) VALUES ('BBB')"));
	check_call(run_command, (cmd, "INSERT #test_table (col1) VALUES ('CCC')"));

	check_call(run_command, (cmd, "CREATE TABLE #test_table2 (col1 char(4))"));
	check_call(run_command, (cmd, "INSERT #test_table2 (col1) VALUES ('XXX')"));
	check_call(run_command, (cmd, "INSERT #test_table2 (col1) VALUES ('YYY')"));
	check_call(run_command, (cmd, "INSERT #test_table2 (col1) VALUES ('ZZZ')"));

	if (verbose) {
		printf("opening first cursor on connection\n");
	}

	strcpy(text, "select col1 from #test_table order by col1");

	check_call(ct_cursor, (cmd, CS_CURSOR_DECLARE, name, CS_NULLTERM, text, CS_NULLTERM, CS_UNUSED));

	check_call(ct_cursor, (cmd, CS_CURSOR_ROWS, name, CS_NULLTERM, NULL, CS_UNUSED, (CS_INT) 1));

	check_call(ct_cursor, (cmd, CS_CURSOR_OPEN, name, CS_NULLTERM, text, CS_NULLTERM, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch ((int) result_type) {

		case CS_CMD_FAIL:
			fprintf(stderr, "ct_results failed\n");
			return 1;
		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
		case CS_STATUS_RESULT:
			break;

		case CS_CURSOR_RESULT:
			check_call(ct_res_info, (cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));

			if (num_cols != 1) {
				fprintf(stderr, "unexpected num of columns =  %d \n", num_cols);
				return 1;
			}

			for (i = 0; i < num_cols; i++) {

				/* here we can finally test for the return status column */
				check_call(ct_describe, (cmd, i + 1, &datafmt));

				if (datafmt.status & CS_RETURN) {
					printf("ct_describe() column %d \n", i);
				}

				datafmt.datatype = CS_CHAR_TYPE;
				datafmt.format = CS_FMT_NULLTERM;
				datafmt.maxlength = 6;
				datafmt.count = 1;
				datafmt.locale = NULL;
				check_call(ct_bind, (cmd, 1, &datafmt, col1, &datalength, &ind));

			}
			break;

		case CS_COMPUTE_RESULT:
			fprintf(stderr, "ct_results() unexpected CS_COMPUTE_RESULT.\n");
			return 1;
		default:
			fprintf(stderr, "ct_results() unexpected result_type.\n");
			return 1;
		}
	}

	if (verbose) {
		printf("opening second cursor on connection\n");
	}

	strcpy(text, "select col1 from #test_table2 order by col1");

	check_call(ct_cursor, (cmd2, CS_CURSOR_DECLARE, name2, CS_NULLTERM, text, CS_NULLTERM, CS_UNUSED));

	check_call(ct_cursor, (cmd2, CS_CURSOR_ROWS, name2, CS_NULLTERM, NULL, CS_UNUSED, (CS_INT) 1));

	check_call(ct_cursor, (cmd2, CS_CURSOR_OPEN, name2, CS_NULLTERM, text, CS_NULLTERM, CS_UNUSED));

	check_call(ct_send, (cmd2));

	while ((results_ret = ct_results(cmd2, &result_type)) == CS_SUCCEED) {
		switch ((int) result_type) {

		case CS_CMD_FAIL:
			fprintf(stderr, "ct_results failed\n");
			return 1;
		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
		case CS_STATUS_RESULT:
			break;

		case CS_CURSOR_RESULT:
			check_call(ct_res_info, (cmd2, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));

			if (num_cols != 1) {
				fprintf(stderr, "unexpected num of columns =  %d \n", num_cols);
				return 1;
			}

			for (i = 0; i < num_cols; i++) {

				/* here we can finally test for the return status column */
				check_call(ct_describe, (cmd2, i + 1, &datafmt2));

				if (datafmt2.status & CS_RETURN) {
					printf("ct_describe() column %d \n", i);
				}

				datafmt2.datatype = CS_CHAR_TYPE;
				datafmt2.format = CS_FMT_NULLTERM;
				datafmt2.maxlength = 6;
				datafmt2.count = 1;
				datafmt2.locale = NULL;
				check_call(ct_bind, (cmd2, 1, &datafmt2, col2, &datalength, &ind));
			}
			break;

		case CS_COMPUTE_RESULT:
			fprintf(stderr, "ct_results() unexpected CS_COMPUTE_RESULT.\n");
			return 1;
		default:
			fprintf(stderr, "ct_results() unexpected result_type.\n");
			return 1;
		}
	}

	row_count = 0;

	while (((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
	       || (ret == CS_ROW_FAIL)) {

		ret2 = ct_fetch(cmd2, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count2);
		if (ret == CS_SUCCEED && ret2 == CS_SUCCEED) {
			printf("%s\t\t\t%s\n", col1, col2);
		}
	}

	switch ((int) ret) {
	case CS_END_DATA:
		break;
	case CS_ROW_FAIL:
		fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
		return 1;
	case CS_FAIL:
		fprintf(stderr, "ct_fetch() returned CS_FAIL.\n");
		return 1;
	default:
		fprintf(stderr, "ct_fetch() unexpected return.\n");
		return 1;
	}

	if (verbose) {
		printf("closing first cursor on connection\n");
	}

	check_call(ct_cursor, (cmd, CS_CURSOR_CLOSE, name, CS_NULLTERM, NULL, CS_UNUSED, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		if (result_type == CS_CMD_FAIL) {
			fprintf(stderr, "ct_results(2) result_type CS_CMD_FAIL.\n");
			return 1;
		}
	}
	if (results_ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results() returned BAD.\n");
		return 1;
	}

	check_call(ct_cursor, (cmd, CS_CURSOR_DEALLOC, name, CS_NULLTERM, NULL, CS_UNUSED, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		if (result_type == CS_CMD_FAIL) {
			fprintf(stderr, "ct_results(3) result_type CS_CMD_FAIL.\n");
			return 1;
		}
	}
	if (results_ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results() returned BAD.\n");
		return 1;
	}

	if (verbose) {
		printf("closing second cursor on connection\n");
	}

	check_call(ct_cursor, (cmd2, CS_CURSOR_CLOSE, name2, CS_NULLTERM, NULL, CS_UNUSED, CS_UNUSED));

	check_call(ct_send, (cmd2));

	while ((results_ret = ct_results(cmd2, &result_type)) == CS_SUCCEED) {
		if (result_type == CS_CMD_FAIL) {
			fprintf(stderr, "ct_results(2) result_type CS_CMD_FAIL.\n");
			return 1;
		}
	}
	if (results_ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results() returned BAD.\n");
		return 1;
	}

	check_call(ct_cursor, (cmd2, CS_CURSOR_DEALLOC, name2, CS_NULLTERM, NULL, CS_UNUSED, CS_UNUSED));

	check_call(ct_send, (cmd2));

	while ((results_ret = ct_results(cmd2, &result_type)) == CS_SUCCEED) {
		if (result_type == CS_CMD_FAIL) {
			fprintf(stderr, "ct_results(3) result_type CS_CMD_FAIL.\n");
			return 1;
		}
	}
	if (results_ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results() returned BAD.\n");
		return 1;
	}

	ct_cmd_drop(cmd2);
	ct_cmd_drop(cmd3);

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	if (verbose) {
		printf("Test succeded\n");
	}
	return 0;
}
