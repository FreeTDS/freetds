#include "common.h"

/* protos */
int do_fetch(CS_COMMAND * cmd);
CS_RETCODE do_results(CS_COMMAND * cmd, CS_INT * results);

/* defines */
#define NUMROWS 5

/* Testing: Test order of ct_results() */
int
main(void)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int i, verbose = 0;

	CS_RETCODE results_ret;

	char query[1024];
	CS_INT insert_results[] = { CS_CMD_SUCCEED, CS_CMD_DONE };
	CS_INT update_results[] = { CS_CMD_SUCCEED, CS_CMD_DONE };
	CS_INT select_results[] = { CS_ROW_RESULT, CS_CMD_DONE };

	printf("%s: Check ordering of returns from cs_results()\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(run_command, (cmd, "CREATE TABLE #t0004 (id int)"));
	for (i = 0; i < NUMROWS; i++) {
		sprintf(query, "INSERT #t0004 (id) VALUES (%d)", i);

		check_call(ct_command, (cmd, CS_LANG_CMD, query, CS_NULLTERM, CS_UNUSED));
		check_call(ct_send, (cmd));

		results_ret = do_results(cmd, insert_results);
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
	}

	check_call(ct_command, (cmd, CS_LANG_CMD, "UPDATE #t0004 SET id = id + 1", CS_NULLTERM, CS_UNUSED));
	check_call(ct_send, (cmd));

	results_ret = do_results(cmd, update_results);
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

	/* single row select */
	check_call(ct_command, (cmd, CS_LANG_CMD, "SELECT * FROM #t0004 WHERE id = 1", CS_NULLTERM, CS_UNUSED));
	check_call(ct_send, (cmd));

	results_ret = do_results(cmd, select_results);
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

int
do_fetch(CS_COMMAND * cmd)
{
CS_INT count, row_count = 0;
CS_RETCODE ret;

	while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED) {
		row_count += count;
	}
	if (ret == CS_ROW_FAIL) {
		fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
		return 1;
	} else if (ret == CS_END_DATA) {
		return 0;
	} else {
		fprintf(stderr, "ct_fetch() unexpected return %d on row %d.\n", ret, row_count);
		return 1;
	}
}

CS_RETCODE
do_results(CS_COMMAND * cmd, CS_INT * results)
{
int result_num;
CS_RETCODE results_ret, result_type;

	result_num = 0;
	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		printf("result_ret %d result_type %d\n", results_ret, result_type);
		if (result_type == CS_STATUS_RESULT)
			continue;
		if (result_type != results[result_num]) {
			fprintf(stderr, "ct_results() expected %d received %d\n", results[result_num], result_type);
			return CS_FAIL;
		}
		switch ((int) result_type) {
		case CS_ROW_RESULT:
			if (do_fetch(cmd)) {
				return CS_FAIL;
			}
			break;
		}
		result_num++;
	}
	return results_ret;
}
