#include "common.h"

/* Testing: data truncation behavior of ct_fetch */
int
main(int argc, char *argv[])
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
	CS_INT copied = 0;
	CS_SMALLINT ind = 0;
	CS_INT count, row_count = 0;

	CS_CHAR select[1024];

	char *addr = NULL;

	printf("%s: test data truncation behavior of ct_fetch \n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin_with_options, (argc, argv, &ctx, &conn, &cmd, verbose));
	verbose += common_pwd.fverbose;

	strcpy(select, "select name from systypes where datalength(name) > 2*9 order by datalength(name)");
	printf("%s\n", select);

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
			free(addr);
			return 1;
		case CS_ROW_RESULT:

			check_call(ct_res_info, (cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));
			fprintf(stderr, "%d column%s\n", num_cols, num_cols==1? "":"s");

			check_call(ct_describe, (cmd, 1, &datafmt));

			fprintf(stderr, "type = %d\n", datafmt.datatype);
			fprintf(stderr, "maxlength = %d\n", datafmt.maxlength);
			if (datafmt.datatype == CS_CHAR_TYPE) {
				fprintf(stderr, "CS_CHAR_TYPE\n");
				datafmt.format = CS_FMT_NULLTERM;
				addr = (char *) malloc(datafmt.maxlength);
			}

			fprintf(stderr, "binding column 1 (%s)\n", datafmt.name);

			/* set maxlength to something short to test truncation behavior */
			if (common_pwd.maxlength) 
				datafmt.maxlength = common_pwd.maxlength;

			check_call(ct_bind, (cmd, 1, &datafmt, addr, &copied, &ind));

			fprintf(stderr, "fetching rows with datafmt.maxlength = %d\n", datafmt.maxlength);

			while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) != CS_END_DATA) {

				fprintf(stderr, "ct_fetch() row %d returned %d.\n", row_count, (int) ret);
				addr[copied] = '\0';
				fprintf(stderr, "copied %d bytes: [%s]\n", copied, addr);
				row_count += count;

				switch (ret) {
				case CS_SUCCEED:
					printf("ct_fetch returned %d row%s\n", count, count==1? "":"s");
					break;
				case CS_ROW_FAIL:
					fprintf(stderr, "error: ct_fetch() returned CS_ROW_FAIL on row %d.\n", row_count);
					free(addr);
					return 1;
				case CS_CANCELED:
					fprintf(stderr, "error: ct_fetch() returned CS_CANCELED??\n");
					free(addr);
					return 1;
				case CS_FAIL:
					fprintf(stderr, "error: ct_fetch() returned CS_FAIL.\n");
					free(addr);
					return 1;
				default:
					fprintf(stderr, "error: ct_fetch() unexpected return.\n");
					free(addr);
					return 1;
				}
			}
			
			break;
		}
	}
	
	switch ((int) results_ret) {
	case CS_END_RESULTS:
		break;
	case CS_FAIL:
		fprintf(stderr, "ct_results() failed.\n");
		free(addr);
		return 1;
		break;
	default:
		fprintf(stderr, "ct_results() unexpected return.\n");
		free(addr);
		return 1;
	}

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	free(addr);
	return 0;
}
