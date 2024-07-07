#include "common.h"

static CS_RETCODE ex_servermsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_SERVERMSG * errmsg);
static int compute_supported = 1;

/* Testing: Retrieve compute results */
int
main(void)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int verbose = 0;

	CS_RETCODE ret;
	CS_RETCODE results_ret;
	CS_INT result_type;
	CS_INT num_cols, compute_id;

	CS_DATAFMT datafmt;
	CS_INT datalength;
	CS_SMALLINT ind;
	CS_INT count, row_count = 0;

	CS_CHAR select[1024];

	CS_INT col1;
	CS_CHAR col2[2];
	CS_CHAR col3[32];

	CS_INT compute_col1;
	CS_CHAR compute_col3[32];

	unsigned rows[3] = { 0, 0, 0 };

	printf("%s: Retrieve compute results processing\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(run_command,
		   (cmd, "CREATE TABLE #ctlib0009 (col1 int not null,  col2 char(1) not null, col3 datetime not null)"));

	check_call(run_command, (cmd, "insert into #ctlib0009 values (1, 'A', 'Jan  1 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlib0009 values (2, 'A', 'Jan  2 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlib0009 values (3, 'A', 'Jan  3 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlib0009 values (8, 'B', 'Jan  4 2002 10:00:00AM')"));
	check_call(run_command, (cmd, "insert into #ctlib0009 values (9, 'B', 'Jan  5 2002 10:00:00AM')"));

	strcpy(select, "select col1, col2, col3 from #ctlib0009 order by col2 ");
	strcat(select, "compute sum(col1) by col2 ");
	strcat(select, "compute max(col3)");

	check_call(ct_command, (cmd, CS_LANG_CMD, select, CS_NULLTERM, CS_UNUSED));

	check_call(ct_send, (cmd));

	ct_callback(ctx, NULL, CS_SET, CS_SERVERMSG_CB, (CS_VOID *) ex_servermsg_cb);
	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		printf("ct_results returned %s type\n", res_type_str(result_type));
		switch ((int) result_type) {
		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_CMD_FAIL:
			if (!compute_supported) {
				try_ctlogout(ctx, conn, cmd, verbose);
				return 0;
			}
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
			check_call(ct_bind, (cmd, 1, &datafmt, &col1, &datalength, &ind));

			check_call(ct_describe, (cmd, 2, &datafmt));

			datafmt.format = CS_FMT_NULLTERM;
			datafmt.maxlength = 2;

			check_call(ct_bind, (cmd, 2, &datafmt, col2, &datalength, &ind));

			check_call(ct_describe, (cmd, 3, &datafmt));

			datafmt.datatype = CS_CHAR_TYPE;
			datafmt.format = CS_FMT_NULLTERM;
			datafmt.maxlength = 32;

			check_call(ct_bind, (cmd, 3, &datafmt, col3, &datalength, &ind));

			while (((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				if (ret == CS_ROW_FAIL) {
					fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
					return 1;
				} else {	/* ret == CS_SUCCEED */
					printf("col1 = %d col2= '%s', col3 = '%s'\n", col1, col2, col3);
					++rows[0];
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

			printf("testing compute_result\n");

			check_call(ct_compute_info, (cmd, CS_COMP_ID, CS_UNUSED, &compute_id, CS_UNUSED, NULL));

			if (compute_id != 1 && compute_id != 2) {
				fprintf(stderr, "invalid compute_id value");
				return 1;
			}

			if (compute_id == 1) {
				check_call(ct_res_info, (cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));
				if (num_cols != 1) {
					fprintf(stderr, "compute_id %d num_cols %d != 1", compute_id, num_cols);
					return 1;
				}

				check_call(ct_describe, (cmd, 1, &datafmt));
				datafmt.format = CS_FMT_UNUSED;
				if (datafmt.maxlength > 1024) {
					datafmt.maxlength = 1024;
				}
				compute_col1 = -1;
				check_call(ct_bind, (cmd, 1, &datafmt, &compute_col1, &datalength, &ind));
			}
			if (compute_id == 2) {
				check_call(ct_res_info, (cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));
				if (num_cols != 1) {
					fprintf(stderr, "compute_id %d num_cols %d != 1", compute_id, num_cols);
					return 1;
				}

				check_call(ct_describe, (cmd, 1, &datafmt));

				datafmt.datatype = CS_CHAR_TYPE;
				datafmt.format = CS_FMT_NULLTERM;
				datafmt.maxlength = 32;

				compute_col3[0] = 0;
				check_call(ct_bind, (cmd, 1, &datafmt, compute_col3, &datalength, &ind));
			}


			while (((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				if (ret == CS_ROW_FAIL) {
					fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
					return 1;
				} else {	/* ret == CS_SUCCEED */
					if (compute_id == 1) {
						printf("compute_col1 = %d \n", compute_col1);
						if (compute_col1 != 6 && compute_col1 != 17) {
							fprintf(stderr, "(should be 6 or 17)\n");
							return 1;
						}
					}
					if (compute_id == 2) {
						printf("compute_col3 = '%s'\n", compute_col3);
						if (strcmp("Jan  5 2002 10:00:00AM", compute_col3)
						    && strcmp("Jan 05 2002 10:00AM", compute_col3)
						    && strcmp("Jan  5 2002 10:00AM", compute_col3)) {
							fprintf(stderr, "(should be \"Jan  5 2002 10:00:00AM\")\n");
							return 1;
						}
					}
					++rows[compute_id];
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

	if (rows[0] != 5 || rows[1] != 2 || rows[2] != 1) {
		fprintf(stderr, "wrong number of rows: normal %u compute_1 %u compute_2 %u, expected 5 2 1\n",
			rows[0], rows[1], rows[2]);
		return 1;
	}

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;
}

static CS_RETCODE
ex_servermsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_SERVERMSG * srvmsg)
{
	if (strstr(srvmsg->text, "compute")) {
		compute_supported = 0;
		printf("Server does not support compute\n");
		return CS_SUCCEED;
	}
	return servermsg_cb(context, connection, srvmsg);
}
