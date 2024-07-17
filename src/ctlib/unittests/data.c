/* Test some data from server. Currently tests MS XML type */
#include "common.h"

TEST_MAIN()
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int verbose = 0;

	CS_RETCODE ret;
	CS_INT result_type;
	CS_INT num_cols;

	CS_DATAFMT datafmt;
	CS_INT copied = 0;
	CS_SMALLINT ind = 0;
	CS_INT count;

	const char *select;

	char buffer[128];

	int tds_version;

	printf("%s: test data from server\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin_with_options, (argc, argv, &ctx, &conn, &cmd, verbose));
	verbose += common_pwd.fverbose;

	ret = ct_con_props(conn, CS_GET, CS_TDS_VERSION, &tds_version, CS_UNUSED, NULL);
	if (ret == CS_SUCCEED) {
		switch (tds_version) {
		case CS_TDS_70:
		case CS_TDS_71:
			fprintf(stderr, "This TDS version does not support XML.\n");
			try_ctlogout(ctx, conn, cmd, verbose);
			return 0;
		}
	}

	select = "select cast('<a b=\"aaa\"><b>ciao</b>hi</a>' as xml) as name";
	printf("%s\n", select);

	check_call(ct_command, (cmd, CS_LANG_CMD, select, CS_NULLTERM, CS_UNUSED));

	check_call(ct_send, (cmd));

	check_call(ct_results, (cmd, &result_type));

	switch (result_type) {
	case CS_CMD_FAIL:
		fprintf(stderr, "ct_results() result_type CS_CMD_FAIL, probably not MSSQL.\n");
		try_ctlogout(ctx, conn, cmd, verbose);
		return 0;
	case CS_ROW_RESULT:
		break;
	default:
		fprintf(stderr, "ct_results() unexpected return %d.\n", result_type);
		goto Cleanup;
	}

	check_call(ct_res_info, (cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));
	assert(num_cols == 1);

	check_call(ct_describe, (cmd, 1, &datafmt));

	assert(strcmp(datafmt.name, "name") == 0);
	assert(datafmt.datatype == CS_LONGCHAR_TYPE);
	assert(datafmt.maxlength == 0x7fffffff);
	assert(datafmt.scale == 0);
	assert(datafmt.precision == 0);

	datafmt.format = CS_FMT_NULLTERM;
	datafmt.maxlength = 100;

	printf("binding column 1\n");
	check_call(ct_bind, (cmd, 1, &datafmt, buffer, &copied, &ind));

	printf("fetching rows.\n");
	while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED) {
		fprintf(stderr, "ct_fetch() returned %d.\n", (int) ret);
		buffer[copied] = '\0';
		fprintf(stderr, "copied %d bytes: [%s]\n", copied, buffer);
	}
	assert(ret == CS_END_DATA);

	do {
		ret = ct_results(cmd, &result_type);
	} while (ret == CS_SUCCEED);

	assert(ret == CS_END_RESULTS);

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return 0;

Cleanup:
	try_ctlogout(ctx, conn, cmd, verbose);
	return 1;
}
