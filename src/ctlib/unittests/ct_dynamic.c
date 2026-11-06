#include "common.h"

#include <stdarg.h>

static bool verbose = false;

static CS_CONTEXT *ctx;
static CS_CONNECTION *conn;
static CS_COMMAND *cmd;
static CS_COMMAND *cmd2;

static void
cleanup(void)
{
	if (verbose) {
		printf("Trying logout\n");
	}

	if (cmd2)
		ct_cmd_drop(cmd2);

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));
}

static void
chk(int check, const char *fmt, ...)
{
	va_list ap;

	if (check)
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	cleanup();
	exit(1);
}

enum
{ BIG_SIZE = 20000 };

static void
consume_results(CS_COMMAND *command, const char *what)
{
	CS_RETCODE ret;
	CS_INT res_type;

	while ((ret = ct_results(command, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {
		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;
		default:
			chk(0, "%s: unexpected ct_results result type: %d\n", what, (int) res_type);
		}
	}
	chk(ret == CS_END_RESULTS, "%s: ct_results() unexpected return %d.\n", what, (int) ret);
}

/*
 * Prepare an INSERT with a blob parameter and execute it with a mix
 * of NULL and not NULL values, then check what got stored.
 */
static void
test_blob_param(const char *sql_type, CS_INT param_type)
{
	enum
	{ NUM_ROWS = 4 };
	static const int sizes[NUM_ROWS] = { -1, 5, -1, BIG_SIZE };
	char sql[256];
	char *data, *out;
	CS_DATAFMT datafmt;
	CS_INT id, id_len, data_len;
	CS_SMALLINT id_ind, data_ind;
	CS_RETCODE ret;
	CS_INT res_type, count;
	int i, rows = 0;

	printf("Testing blob parameter of type %s\n", sql_type);

	data = (char *) malloc(BIG_SIZE);
	out = (char *) malloc(BIG_SIZE);
	chk(data != NULL && out != NULL, "out of memory\n");
	memset(data, 'x', BIG_SIZE);

	run_command(cmd, "IF OBJECT_ID('tempdb..#ct_dynamic_blob') IS NOT NULL DROP TABLE #ct_dynamic_blob");
	sprintf(sql, "CREATE TABLE #ct_dynamic_blob (id INT NOT NULL, t %s NULL)", sql_type);
	check_call(run_command, (cmd, sql));

	check_call(ct_dynamic, (cmd, CS_PREPARE, "blob", CS_NULLTERM,
				"INSERT INTO #ct_dynamic_blob (id, t) VALUES (?, ?)", CS_NULLTERM));
	check_call(ct_send, (cmd));
	consume_results(cmd, "prepare");

	for (i = 0; i < NUM_ROWS; i++) {
		check_call(ct_dynamic, (cmd, CS_EXECUTE, "blob", CS_NULLTERM, NULL, CS_UNUSED));

		id = i + 1;
		id_len = sizeof(id);
		id_ind = 0;
		memset(&datafmt, 0, sizeof(datafmt));
		datafmt.datatype = CS_INT_TYPE;
		datafmt.status = CS_INPUTVALUE;
		check_call(ct_setparam, (cmd, &datafmt, &id, &id_len, &id_ind));

		data_len = sizes[i] < 0 ? 0 : sizes[i];
		data_ind = sizes[i] < 0 ? -1 : 0;
		memset(&datafmt, 0, sizeof(datafmt));
		datafmt.datatype = param_type;
		datafmt.status = CS_INPUTVALUE;
		datafmt.maxlength = BIG_SIZE;
		check_call(ct_setparam, (cmd, &datafmt, data, &data_len, &data_ind));

		check_call(ct_send, (cmd));
		consume_results(cmd, "execute");
	}

	check_call(ct_dynamic, (cmd, CS_DEALLOC, "blob", CS_NULLTERM, NULL, CS_UNUSED));
	check_call(ct_send, (cmd));
	consume_results(cmd, "deallocate");

	/*
	 * Check stored values.
	 * Fetch the column itself as ASE 15.0 returns 0 for DATALENGTH of a
	 * NULL TEXT and does not accept IS NULL on it.
	 */
	check_call(ct_command, (cmd, CS_LANG_CMD, "SET TEXTSIZE 65536 SELECT id, t FROM #ct_dynamic_blob ORDER BY id",
				CS_NULLTERM, CS_UNUSED));
	check_call(ct_send, (cmd));
	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {
		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;
		case CS_ROW_RESULT:
			memset(&datafmt, 0, sizeof(datafmt));
			datafmt.datatype = CS_INT_TYPE;
			datafmt.maxlength = sizeof(CS_INT);
			datafmt.count = 1;
			check_call(ct_bind, (cmd, 1, &datafmt, &id, NULL, &id_ind));
			memset(&datafmt, 0, sizeof(datafmt));
			datafmt.datatype = param_type;
			datafmt.maxlength = BIG_SIZE;
			datafmt.count = 1;
			check_call(ct_bind, (cmd, 2, &datafmt, out, &data_len, &data_ind));
			memset(out, 0, BIG_SIZE);
			while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED) {
				chk(rows < NUM_ROWS && id == rows + 1, "%s: unexpected row id %d\n", sql_type, (int) id);
				if (sizes[rows] < 0)
					chk(data_ind == -1, "%s: row %d expected NULL\n", sql_type, (int) id);
				else
					chk(data_ind == 0 && data_len == sizes[rows] && memcmp(out, data, data_len) == 0,
					    "%s: row %d wrong data, length %d expected %d\n", sql_type, (int) id,
					    (int) data_len, sizes[rows]);
				memset(out, 0, BIG_SIZE);
				++rows;
			}
			chk(ret == CS_END_DATA, "ct_fetch() unexpected return %d.\n", (int) ret);
			break;
		default:
			chk(0, "invalid ct_results result type: %d\n", (int) res_type);
		}
	}
	chk(ret == CS_END_RESULTS, "ct_results() unexpected return %d.\n", (int) ret);
	chk(rows == NUM_ROWS, "%s: got %d rows, expected %d\n", sql_type, rows, NUM_ROWS);

	check_call(run_command, (cmd, "DROP TABLE #ct_dynamic_blob"));
	free(out);
	free(data);
}

TEST_MAIN()
{
	CS_RETCODE ret;
	CS_RETCODE results_ret;
	CS_CHAR cmdbuf[4096];
	CS_CHAR name[257];
	CS_INT datalength;
	CS_SMALLINT ind;
	CS_INT count;
	CS_INT num_cols;
	CS_INT row_count = 0;
	CS_DATAFMT datafmt;
	CS_DATAFMT descfmt;
	CS_INT intvar;
	CS_INT intvarsize;
	CS_SMALLINT intvarind;
	CS_INT res_type;
	CS_INT tds_version;
	bool test_blobs;

	int i;

	if (argc > 1 && (0 == strcmp(argv[1], "-v")))
		verbose = true;

	printf("%s: use ct_dynamic to prepare and execute  a statement\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));
	error_to_stdout = true;

	check_call(ct_cmd_alloc, (conn, &cmd2));

	/* do not test error */
	run_command(cmd, "IF OBJECT_ID('tempdb..#ct_dynamic') IS NOT NULL DROP table #ct_dynamic");

	strcpy(cmdbuf, "create table #ct_dynamic (id numeric identity not null, \
        name varchar(30), age int, cost money, bdate datetime, fval float) ");

	check_call(run_command, (cmd, cmdbuf));

	strcpy(cmdbuf, "insert into #ct_dynamic ( name , age , cost , bdate , fval ) ");
	strcat(cmdbuf, "values ('Bill', 44, 2000.00, 'May 21 1960', 60.97 ) ");

	check_call(run_command, (cmd, cmdbuf));

	strcpy(cmdbuf, "insert into #ct_dynamic ( name , age , cost , bdate , fval ) ");
	strcat(cmdbuf, "values ('Freddy', 32, 1000.00, 'Jan 21 1972', 70.97 ) ");

	check_call(run_command, (cmd, cmdbuf));

	strcpy(cmdbuf, "insert into #ct_dynamic ( name , age , cost , bdate , fval ) ");
	strcat(cmdbuf, "values ('James', 42, 5000.00, 'May 21 1962', 80.97 ) ");

	check_call(run_command, (cmd, cmdbuf));

	strcpy(cmdbuf, "select name from #ct_dynamic where age = ?");

	check_call(ct_dynamic, (cmd, CS_PREPARE, "age", CS_NULLTERM, cmdbuf, CS_NULLTERM));

	check_call(ct_send, (cmd));

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			break;

		default:
			chk(0, "invalid ct_results result type: %d\n", res_type);
		}
	}
	chk(ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) ret);

	check_call(ct_dynamic, (cmd, CS_DESCRIBE_INPUT, "age", CS_NULLTERM, NULL, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_DESCRIBE_RESULT:
			check_call(ct_res_info, (cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));

			for (i = 1; i <= num_cols; i++) {
				check_call(ct_describe, (cmd, i, &descfmt));
				fprintf(stderr, "CS_DESCRIBE_INPUT parameter %d :\n", i);
				if (descfmt.namelen == 0)
					fprintf(stderr, "\t\tNo name...\n");
				else
					fprintf(stderr, "\t\tName = %*.*s\n", descfmt.namelen, descfmt.namelen, descfmt.name);
				fprintf(stderr, "\t\tType   = %d\n", descfmt.datatype);
				fprintf(stderr, "\t\tLength = %d\n", descfmt.maxlength);
			}
			break;

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			break;

		default:
			chk(0, "invalid ct_results result type: %d\n", res_type);
		}
	}

	check_call(ct_dynamic, (cmd, CS_DESCRIBE_OUTPUT, "age", CS_NULLTERM, NULL, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_DESCRIBE_RESULT:
			check_call(ct_res_info, (cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));
			chk(num_cols == 1, "CS_DESCRIBE_OUTPUT showed %d columns , expected 1\n", num_cols);

			for (i = 1; i <= num_cols; i++) {
				check_call(ct_describe, (cmd, i, &descfmt));

				if (descfmt.namelen == 0)
					fprintf(stderr, "\t\tNo name...\n");
				else
					fprintf(stderr, "\t\tName = %*.*s\n", descfmt.namelen, descfmt.namelen, descfmt.name);
				fprintf(stderr, "\t\tType   = %d\n", descfmt.datatype);
				fprintf(stderr, "\t\tLength = %d\n", descfmt.maxlength);
			}
			break;

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			break;

		default:
			chk(0, "invalid ct_results result type: %d\n", res_type);
		}
	}

	/* execute dynamic on a second command to check it still works */
	check_call(ct_dynamic, (cmd2, CS_EXECUTE, "age", CS_NULLTERM, NULL, CS_UNUSED));

	intvar = 44;
	intvarsize = 4;
	intvarind = 0;

	datafmt.name[0] = 0;
	datafmt.namelen = 0;
	datafmt.datatype = CS_INT_TYPE;
	datafmt.status = CS_INPUTVALUE;

	check_call(ct_setparam, (cmd2, &datafmt, (CS_VOID *) & intvar, &intvarsize, &intvarind));

	check_call(ct_send, (cmd2));

	while ((results_ret = ct_results(cmd2, &res_type)) == CS_SUCCEED) {
		chk(res_type != CS_CMD_FAIL, "1: ct_results() result_type CS_CMD_FAIL.\n");
		chk(res_type != CS_COMPUTE_RESULT, "ct_results() unexpected CS_COMPUTE_RESULT.\n");

		switch ((int) res_type) {
		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_ROW_RESULT:
			datafmt.datatype = CS_CHAR_TYPE;
			datafmt.format = CS_FMT_NULLTERM;
			datafmt.maxlength = 256;
			datafmt.count = 1;
			datafmt.locale = NULL;
			check_call(ct_bind, (cmd2, 1, &datafmt, name, &datalength, &ind));

			while (((ret = ct_fetch(cmd2, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				chk(ret != CS_ROW_FAIL, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
				if (ret == CS_SUCCEED) {
					chk(!strcmp(name, "Bill"), "fetched value '%s' expected 'Bill'\n", name);
				} else {
					break;
				}
			}
			chk(ret == CS_END_DATA, "ct_fetch() unexpected return %d.\n", (int) ret);
			break;
		default:
			fprintf(stderr, "ct_results() unexpected result_type.\n");
			return 1;
		}
	}
	chk(results_ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) results_ret);

	intvar = 32;

	check_call(ct_send, (cmd2));

	while ((results_ret = ct_results(cmd2, &res_type)) == CS_SUCCEED) {
		chk(res_type != CS_CMD_FAIL, "2: ct_results() result_type CS_CMD_FAIL.\n");
		chk(res_type != CS_COMPUTE_RESULT, "ct_results() unexpected CS_COMPUTE_RESULT.\n");

		switch ((int) res_type) {
		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_ROW_RESULT:
			datafmt.datatype = CS_CHAR_TYPE;
			datafmt.format = CS_FMT_NULLTERM;
			datafmt.maxlength = 256;
			datafmt.count = 1;
			datafmt.locale = NULL;
			check_call(ct_bind, (cmd2, 1, &datafmt, name, &datalength, &ind));

			while (((ret = ct_fetch(cmd2, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				chk(ret != CS_ROW_FAIL, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);

				if (ret == CS_SUCCEED) {
					chk(!strcmp(name, "Freddy"), "fetched value '%s' expected 'Freddy'\n", name);
				} else {
					break;
				}
			}
			chk(ret == CS_END_DATA, "ct_fetch() unexpected return %d.\n", (int) ret);
			break;
		default:
			fprintf(stderr, "ct_results() unexpected result_type.\n");
			return 1;
		}
	}
	chk(results_ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) results_ret);

	check_call(ct_dynamic, (cmd, CS_DEALLOC, "age", CS_NULLTERM, NULL, CS_UNUSED));

	check_call(ct_send, (cmd));

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			break;

		default:
			chk(0, "invalid ct_results result type: %d\n", res_type);
		}
	}
	chk(ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) ret);

	/*
	 * check we can prepare again dynamic with same name after deallocation
	 */
	strcpy(cmdbuf, "select name from #ct_dynamic where age = ?");
	check_call(ct_dynamic, (cmd, CS_PREPARE, "age", CS_NULLTERM, cmdbuf, CS_NULLTERM));

	check_call(ct_send, (cmd));

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			break;

		default:
			chk(0, "invalid ct_results result type: %d\n", res_type);
		}
	}
	chk(ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) ret);

	ct_dynamic(cmd, CS_DEALLOC, "invalid", CS_NULLTERM, NULL, CS_UNUSED);
	ct_send(cmd);
	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED)
		chk(res_type != CS_ROW_RESULT, "Rows not expected\n");
	chk(ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) ret);

	/*
	 * Blob parameters, mixing NULL and not NULL values.
	 * Under MSSQL the statement is prepared before parameters are known.
	 * Before TDS 7.2 they are declared as varchar(4000) so the server
	 * truncates longer values and rejects IMAGE parameters.
	 */
	check_call(ct_con_props, (conn, CS_GET, CS_TDS_VERSION, &tds_version, CS_UNUSED, NULL));
	test_blobs = true;
#ifdef CS_TDS_71
	if (tds_version == CS_TDS_70 || tds_version == CS_TDS_71)
		test_blobs = false;
#endif
	if (test_blobs) {
		test_blob_param("TEXT", CS_TEXT_TYPE);
		test_blob_param("IMAGE", CS_IMAGE_TYPE);
	}

	check_call(ct_dynamic, (cmd2, CS_EXECUTE, "age", CS_NULLTERM, NULL, CS_UNUSED));

	intvar = 32;
	intvarsize = 4;
	intvarind = 0;

	datafmt.name[0] = 0;
	datafmt.namelen = 0;
	datafmt.datatype = CS_INT_TYPE;
	datafmt.status = CS_INPUTVALUE;

	check_call(ct_setparam, (cmd2, &datafmt, (CS_VOID *) & intvar, &intvarsize, &intvarind));
	check_call(ct_send, (cmd2));

	/* all tests succeeded */

	cleanup();
	return 0;
}
