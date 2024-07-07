#include "common.h"

/* Testing: Retrieve CS_TEXT_TYPE using ct_bind() */
int
main(void)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int i, verbose = 0;

	CS_RETCODE ret;
	CS_RETCODE results_ret;
	CS_INT result_type;
	CS_INT num_cols;

	CS_DATAFMT datafmt;
	CS_INT datalength;
	CS_SMALLINT ind;
	CS_INT count, row_count = 0;

	CS_CHAR name[1024];
	char large_sql[1024];
	char len600[601];
	char temp[11];

	len600[0] = 0;
	name[0] = 0;
	for (i = 0; i < 60; i++) {
		sprintf(temp, "_abcde_%03d", (i + 1) * 10);
		strcat(len600, temp);
	}
	len600[600] = '\0';

	printf("%s: Retrieve CS_TEXT_TYPE using ct_bind()\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(run_command, (cmd, "CREATE TABLE #test_table (id int, name text)"));
/*
	check_call(run_command, (cmd, "INSERT #test_table (id, name) VALUES (1, 'name1')"));
*/
	sprintf(large_sql, "INSERT #test_table (id, name) VALUES (2, '%s')", len600);
	check_call(run_command, (cmd, large_sql));

	check_call(ct_command, (cmd, CS_LANG_CMD, "SELECT name FROM #test_table", CS_NULLTERM, CS_UNUSED));
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
			check_call(ct_res_info, (cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));
			if (num_cols != 1) {
				fprintf(stderr, "num_cols %d != 1", num_cols);
				return 1;
			}
			check_call(ct_describe, (cmd, 1, &datafmt));
			datafmt.format = CS_FMT_NULLTERM;
			if (datafmt.maxlength > 1024) {
				datafmt.maxlength = 1024;
			}
			check_call(ct_bind, (cmd, 1, &datafmt, name, &datalength, &ind));

			while (((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				if (ret == CS_ROW_FAIL) {
					fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
					return 1;
				} else {	/* ret == CS_SUCCEED */
					if (verbose) {
						printf("name = '%s'\n", name);
					}
					if (strcmp(name, len600)) {
						fprintf(stderr, "Bad return:\n'%s'\n! =\n'%s'\n", name, len600);
						return 1;
					}
					if (datalength != strlen(name) + 1) {
						fprintf(stderr, "Bad count:\n'%ld'\n! =\n'%d'\n", (long) strlen(name) + 1, count);
						return 1;
					}
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
		case CS_COMPUTE_RESULT:
			fprintf(stderr, "ct_results() unexpected CS_COMPUTE_RESULT.\n");
			return 1;
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

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
