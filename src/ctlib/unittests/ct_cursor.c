#include "common.h"

static int update_second_table(CS_COMMAND * cmd2, char *value);

int
main(void)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	CS_COMMAND *cmd2;
	CS_RETCODE ret;
	CS_RETCODE results_ret;
	CS_INT result_type;
	CS_INT count, row_count = 0;
	CS_DATAFMT datafmt;
	CS_SMALLINT ind;
	int verbose = 1;
	CS_CHAR name[3]; 
	CS_CHAR col1[6];
	CS_INT datalength;
	CS_CHAR text[128];
	CS_INT num_cols, i, j;
	CS_INT props_value;

	printf("%s: Testing ct_cursor()\n", __FILE__);

	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(ct_cmd_alloc, (conn, &cmd2));

	check_call(run_command, (cmd, "CREATE TABLE #test_table (col1 char(4))"));
	check_call(run_command, (cmd, "INSERT #test_table (col1) VALUES ('AAA')"));
	check_call(run_command, (cmd, "INSERT #test_table (col1) VALUES ('BBB')"));
	check_call(run_command, (cmd, "INSERT #test_table (col1) VALUES ('CCC')"));

	check_call(run_command, (cmd2, "CREATE TABLE #test_table2 (col1 char(4))"));
	check_call(run_command, (cmd2, "INSERT #test_table2 (col1) VALUES ('---')"));

	if (verbose) {
		printf("Trying declare, rows , open in one SEND\n");
	}

	strcpy(text, "select col1 from #test_table where 1 = 1");
	strcpy(name, "c1");

	check_call(ct_cursor, (cmd, CS_CURSOR_DECLARE, name, CS_NULLTERM, text, CS_NULLTERM, CS_UNUSED));

	check_call(ct_cursor, (cmd, CS_CURSOR_ROWS, name, CS_NULLTERM, NULL, CS_UNUSED, (CS_INT) 1));

	check_call(ct_cursor, (cmd, CS_CURSOR_OPEN, name, CS_NULLTERM, text, CS_NULLTERM, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch ((int) result_type) {

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
		case CS_CMD_FAIL:
		case CS_STATUS_RESULT:
			break;

		case CS_CURSOR_RESULT:

			check_call(ct_cmd_props, (cmd, CS_GET, CS_CUR_STATUS, &props_value, sizeof(CS_INT), NULL)); 
			if (props_value & CS_CURSTAT_DECLARED) {
				fprintf(stderr, "ct_cmd_props claims cursor is in DECLARED state when it should be OPEN\n");
				return 1;
			}
			if (!(props_value & CS_CURSTAT_OPEN)) {
				fprintf(stderr, "ct_cmd_props claims cursor is not in OPEN state when it should be \n");
				return 1;
			}
			if (props_value & CS_CURSTAT_CLOSED) {
				fprintf(stderr, "ct_cmd_props claims cursor is in CLOSED state when it should be OPEN\n");
				return 1;
			}

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
			row_count = 0;
			while (((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {

				if (row_count == 0) {
					for (j = 0; j < num_cols; j++) {
						printf("\n%s\n", datafmt.name);
					}
					printf("------\n\n");
				}

				for (j = 0; j < num_cols; j++) {
					printf("%s\n\n", col1);
					row_count++;
				}

				ret = update_second_table(cmd2, col1);
				if (ret)
					return ret;
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
				fprintf(stderr, "ct_fetch() unexpected return. %d\n", ret);
				return 1;
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


	check_call(ct_cursor, (cmd, CS_CURSOR_CLOSE, name, CS_NULLTERM, NULL, CS_UNUSED, CS_DEALLOC));

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

	check_call(ct_cmd_props, (cmd, CS_GET, CS_CUR_STATUS, &props_value, sizeof(CS_INT), NULL)); 

	if (props_value != CS_CURSTAT_NONE) {
		fprintf(stderr, "ct_cmd_props() CS_CUR_STATUS != CS_CURSTAT_NONE \n");
		return 1;
	}

	if (verbose) {
		printf("Trying declare, rows, open one at a time \n");
	}

	strcpy(text, "select col1 from #test_table where 2 = 2");

	check_call(ct_cursor, (cmd, CS_CURSOR_DECLARE, name, 3, text, CS_NULLTERM, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		if (result_type == CS_CMD_FAIL) {
			fprintf(stderr, "ct_results(4) result_type CS_CMD_FAIL.\n");
			return 1;
		}
	}
	if (results_ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results() returned BAD.\n");
		return 1;
	}

	check_call(ct_cursor, (cmd, CS_CURSOR_ROWS, name, 3, NULL, CS_UNUSED, (CS_INT) 1));

	check_call(ct_send, (cmd));

	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		if (result_type == CS_CMD_FAIL) {
			fprintf(stderr, "ct_results(5) result_type CS_CMD_FAIL.\n");
			return 1;
		}
	}
	if (results_ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results() returned BAD.\n");
		return 1;
	}

	check_call(ct_cursor, (cmd, CS_CURSOR_OPEN, name, 3, text, 26, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch ((int) result_type) {

		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_CMD_FAIL:
			fprintf(stderr, "ct_results(6) result_type CS_CMD_FAIL.\n");
			break;
		case CS_STATUS_RESULT:
			printf("ct_results: CS_STATUS_RESULT detected for sp_who\n");

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
			row_count = 0;
			while (((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {

				if (row_count == 0) {
					for (j = 0; j < num_cols; j++) {
						printf("\n%s\n", datafmt.name);
					}
					printf("------\n\n");
				}

				for (j = 0; j < num_cols; j++) {
					printf("%s\n\n", col1);
					row_count++;
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
				fprintf(stderr, "ct_fetch() unexpected return. %d\n", ret);
				return 1;
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


	check_call(ct_cursor, (cmd, CS_CURSOR_CLOSE, name, 3, NULL, CS_UNUSED, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		if (result_type == CS_CMD_FAIL) {
			fprintf(stderr, "ct_results(7) result_type CS_CMD_FAIL.\n");
			return 1;
		}
	}
	if (results_ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results() returned BAD.\n");
		return 1;
	}
	check_call(ct_cursor, (cmd, CS_CURSOR_DEALLOC, name, 3, NULL, CS_UNUSED, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		if (result_type == CS_CMD_FAIL) {
			fprintf(stderr, "ct_results(8) result_type CS_CMD_FAIL.\n");
			return 1;
		}
	}
	if (results_ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results() returned BAD.\n");
		return 1;
	}

	if (verbose) {
		printf("Running normal select command after cursor operations\n");
	}

	check_call(ct_command, (cmd, CS_LANG_CMD, "select col1 from #test_table", CS_NULLTERM, CS_UNUSED));
	check_call(ct_send, (cmd));
	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch ((int) result_type) {
		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_CMD_FAIL:
			fprintf(stderr, "ct_results() result_type CS_CMD_FAIL.\n");
			return 1;
		case CS_ROW_RESULT:
			datafmt.datatype = CS_CHAR_TYPE;
			datafmt.format = CS_FMT_NULLTERM;
			datafmt.maxlength = 6;
			datafmt.count = 1;
			datafmt.locale = NULL;
			check_call(ct_bind, (cmd, 1, &datafmt, col1, &datalength, &ind));

			while (((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				if (ret == CS_ROW_FAIL) {
					fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
					return 1;
				} else if (ret == CS_SUCCEED) {
					;
				} else {
					break;
				}
			}
			switch ((int) ret) {
			case CS_END_DATA:
				break;
			case CS_FAIL:
				fprintf(stderr, "ct_fetch() returned CS_FAIL.\n");
				return 1;
			default:
				fprintf(stderr, "ct_fetch() unexpected return.%d\n", ret);
				return 1;
			}
			break;
		case CS_COMPUTE_RESULT:
			fprintf(stderr, "ct_results() unexpected CS_COMPUTE_RESULT.\n");
			return 1;
		default:
			fprintf(stderr, "ct_results() unexpected result_type. %d\n", result_type);
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
	if (verbose) {
		printf("Trying logout\n");
	}

	ct_cmd_drop(cmd2);

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	if (verbose) {
		printf("Test suceeded\n");
	}
	return 0;
}

static int
update_second_table(CS_COMMAND * cmd2, char *value)
{

	CS_RETCODE ret;
	CS_RETCODE results_ret;
	CS_INT result_type;
	CS_INT count, row_count = 0;
	CS_DATAFMT datafmt;
	CS_SMALLINT ind;
	CS_CHAR col1[6];
	CS_INT datalength;
	CS_CHAR text[128];

	sprintf(text, "update #test_table2 set col1 = '%s' ", value);
	ret = run_command(cmd2, text);
	if (ret != CS_SUCCEED)
		return 1;

	ret = ct_command(cmd2, CS_LANG_CMD, "select col1 from #test_table2", CS_NULLTERM, CS_UNUSED);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "ct_command() failed\n");
		return 1;
	}
	ret = ct_send(cmd2);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "ct_send() failed\n");
		return 1;
	}
	while ((results_ret = ct_results(cmd2, &result_type)) == CS_SUCCEED) {
		switch ((int) result_type) {
		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_CMD_FAIL:
			fprintf(stderr, "ct_results() result_type CS_CMD_FAIL.\n");
			return 1;
		case CS_ROW_RESULT:
			datafmt.datatype = CS_CHAR_TYPE;
			datafmt.format = CS_FMT_NULLTERM;
			datafmt.maxlength = 6;
			datafmt.count = 1;
			datafmt.locale = NULL;
			ret = ct_bind(cmd2, 1, &datafmt, col1, &datalength, &ind);
			if (ret != CS_SUCCEED) {
				fprintf(stderr, "ct_bind() failed\n");
				return 1;
			}

			while (((ret = ct_fetch(cmd2, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				if (ret == CS_ROW_FAIL) {
					fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
					return 1;
				} else if (ret == CS_SUCCEED) {
					;
				} else {
					break;
				}
			}
			switch ((int) ret) {
			case CS_END_DATA:
				break;
			case CS_FAIL:
				fprintf(stderr, "ct_fetch() returned CS_FAIL.\n");
				return 1;
			default:
				fprintf(stderr, "ct_fetch() unexpected return.%d\n", ret);
				return 1;
			}
			break;
		case CS_COMPUTE_RESULT:
			fprintf(stderr, "ct_results() unexpected CS_COMPUTE_RESULT.\n");
			return 1;
		default:
			fprintf(stderr, "ct_results() unexpected result_type. %d\n", result_type);
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
	return 0;
}

