#include "common.h"

/* Testing: array binding of result set */
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
	CS_INT datalength[2];
	CS_SMALLINT ind[2];
	CS_INT count, row_count = 0;
	CS_INT cv;

	CS_CHAR select[1024];

	CS_INT col1[2];
	CS_CHAR col2[2][5];
	CS_CHAR col3[2][32];


	printf("%s: Retrieve data using array binding \n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(run_command, (cmd, "CREATE TABLE #ctlibarray (col1 int not null,  col2 char(4) not null, col3 datetime not null)"));
	check_call(run_command, (cmd, "insert into #ctlibarray values (1, 'AAAA', 'Jan  1 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlibarray values (2, 'BBBB', 'Jan  2 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlibarray values (3, 'CCCC', 'Jan  3 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlibarray values (8, 'DDDD', 'Jan  4 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlibarray values (9, 'EEEE', 'Jan  5 2002 10:00:00AM')"));


	strcpy(select, "select col1, col2, col3 from #ctlibarray order by col1 ");

	check_call(ct_command, (cmd, CS_LANG_CMD, select, CS_NULLTERM, CS_UNUSED));

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
			if (num_cols != 3) {
				fprintf(stderr, "num_cols %d != 3", num_cols);
				return 1;
			}

			check_call(ct_describe, (cmd, 1, &datafmt));
			datafmt.format = CS_FMT_UNUSED;
			if (datafmt.maxlength > 1024) {
				datafmt.maxlength = 1024;
			}

			datafmt.count = 2;

			check_call(ct_bind, (cmd, 1, &datafmt, &col1[0], datalength, ind));

			check_call(ct_describe, (cmd, 2, &datafmt));

			datafmt.format = CS_FMT_NULLTERM;
			datafmt.maxlength = 5;
			datafmt.count = 2;

			check_call(ct_bind, (cmd, 2, &datafmt, &col2[0], datalength, ind));

			check_call(ct_describe, (cmd, 3, &datafmt));

			datafmt.datatype = CS_CHAR_TYPE;
			datafmt.format = CS_FMT_NULLTERM;
			datafmt.maxlength = 32;
			datafmt.count = 2;

			check_call(ct_bind, (cmd, 3, &datafmt, &col3[0], datalength, ind));

			count = 0;
			while (((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				if (ret == CS_ROW_FAIL) {
					fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
					return 1;
				} else {	/* ret == CS_SUCCEED */
					printf("ct_fetch returned %d rows\n", count);
					for (cv = 0; cv < count; cv++)
						printf("col1 = %d col2= '%s', col3 = '%s'\n", col1[cv], col2[cv],
							col3[cv]);
				}
				count = 0;
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

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}
