#include "common.h"

#include <stdarg.h>

static const char *query =
	"insert into #ctparam_lang (name,name2,age,cost,bdate,fval) values (@in1, @in2, @in3, @moneyval, @dateval, @floatval)";

static int insert_test(CS_CONNECTION *conn, CS_COMMAND *cmd, int useNames);

/* Testing: binding of data via ct_param */
TEST_MAIN()
{
	int errCode = 0;
	
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int verbose = 0;

	CS_CHAR cmdbuf[4096];

	if (argc > 1 && (0 == strcmp(argv[1], "-v")))
		verbose = 1;

	printf("%s: submit language query with variables using ct_param\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));
	error_to_stdout = true;

	strcpy(cmdbuf, "create table #ctparam_lang (id numeric identity not null, \
		name varchar(30), name2 varchar(20), age int, cost money, bdate datetime, fval float) ");

	check_call(run_command, (cmd, cmdbuf));

	/* test by name */
	errCode = insert_test(conn, cmd, 1);
	/* if worked, test by position */
	if (0 == errCode)
		errCode = insert_test(conn, cmd, 0);
	query = "insert into #ctparam_lang (name,name2,age,cost,bdate,fval) values (?, ?, ?, ?, ?, ?)";
	if (0 == errCode)
		errCode = insert_test(conn, cmd, 0);

	if (verbose && (0 == errCode))
		printf("lang_ct_param tests successful\n");

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	return errCode;
}

static int 
insert_test(CS_CONNECTION *conn, CS_COMMAND *cmd, int useNames)
{
	CS_CONTEXT *ctx;

	CS_RETCODE ret;
	CS_INT res_type;

	CS_DATAFMT datafmt;
	CS_DATAFMT srcfmt;
	CS_DATAFMT destfmt;
	CS_INT intvar;
	CS_FLOAT floatvar;
	CS_MONEY moneyvar;
	CS_DATEREC datevar;
	char moneystring[10];
	char dummy_name[30];
	char dummy_name2[20];
	CS_INT destlen;
	CS_INT rowsAffected = -1;
	int rows_found = 0;

	/* clear table */
	run_command(cmd, "delete #ctparam_lang");

	/*
	 * Assign values to the variables used for parameter passing.
	 */

	intvar = 2;
	floatvar = 0.12;
	strcpy(dummy_name, "joe blow");
	strcpy(dummy_name2, "");
	strcpy(moneystring, "300.90");

	/*
	 * Clear and setup the CS_DATAFMT structures used to convert datatypes.
	 */

	memset(&srcfmt, 0, sizeof(CS_DATAFMT));
	srcfmt.datatype = CS_CHAR_TYPE;
	srcfmt.maxlength = (CS_INT) strlen(moneystring);
	srcfmt.precision = 5;
	srcfmt.scale = 2;
	srcfmt.locale = NULL;

	memset(&destfmt, 0, sizeof(CS_DATAFMT));
	destfmt.datatype = CS_MONEY_TYPE;
	destfmt.maxlength = sizeof(CS_MONEY);
	destfmt.precision = 5;
	destfmt.scale = 2;
	destfmt.locale = NULL;

	/*
	 * Convert the string representing the money value
	 * to a CS_MONEY variable. Since this routine does not have the
	 * context handle, we use the property functions to get it.
	 */
	check_call(ct_cmd_props, (cmd, CS_GET, CS_PARENT_HANDLE, &conn, CS_UNUSED, NULL));
	check_call(ct_con_props, (conn, CS_GET, CS_PARENT_HANDLE, &ctx, CS_UNUSED, NULL));
	check_call(cs_convert, (ctx, &srcfmt, (CS_VOID *) moneystring, &destfmt, &moneyvar, &destlen));

	/*
	 * create the command
	 */
	check_call(ct_command, (cmd, CS_LANG_CMD, (CS_CHAR *) query, (CS_INT) strlen(query), CS_UNUSED));

	/*
	 * Clear and setup the CS_DATAFMT structure, then pass
	 * each of the parameters for the query.
	 */
	memset(&datafmt, 0, sizeof(datafmt));
	if (useNames)
		strcpy(datafmt.name, "@in1");
	else
		datafmt.name[0] = 0;
	datafmt.maxlength = 255;
	datafmt.namelen = CS_NULLTERM;
	datafmt.datatype = CS_CHAR_TYPE;
	datafmt.status = CS_INPUTVALUE;

	/*
	 * The character string variable is filled in by the RPC so pass NULL
	 * for the data 0 for data length, and -1 for the indicator arguments.
	 */
	check_call(ct_param, (cmd, &datafmt, dummy_name, (CS_INT) strlen(dummy_name), 0));

	if (useNames)
		strcpy(datafmt.name, "@in2");
	else
		datafmt.name[0] = 0;
	datafmt.maxlength = 255;
	datafmt.namelen = CS_NULLTERM;
	datafmt.datatype = CS_CHAR_TYPE;
	datafmt.status = CS_INPUTVALUE;

	check_call(ct_param, (cmd, &datafmt, dummy_name2, (CS_INT) strlen(dummy_name2), 0));

	if (useNames)
		strcpy(datafmt.name, "@in3");
	else
		datafmt.name[0] = 0;
	datafmt.namelen = CS_NULLTERM;
	datafmt.datatype = CS_INT_TYPE;
	datafmt.status = CS_INPUTVALUE;

	check_call(ct_param, (cmd, &datafmt, (CS_VOID *) & intvar, CS_SIZEOF(CS_INT), 0));

	if (useNames)
		strcpy(datafmt.name, "@moneyval");
	else
		datafmt.name[0] = 0;
	datafmt.namelen = CS_NULLTERM;
	datafmt.datatype = CS_MONEY_TYPE;
	datafmt.status = CS_INPUTVALUE;

	check_call(ct_param, (cmd, &datafmt, (CS_VOID *) & moneyvar, CS_SIZEOF(CS_MONEY), 0));

	if (useNames)
		strcpy(datafmt.name, "@dateval");
	else
		datafmt.name[0] = 0;
	datafmt.namelen = CS_NULLTERM;
	datafmt.datatype = CS_DATETIME_TYPE;
	datafmt.status = CS_INPUTVALUE;
	memset(&datevar, 0, sizeof(CS_DATEREC));
	datevar.dateyear = 2003;
	datevar.datemonth = 2;
	datevar.datedmonth = 1;

	check_call(ct_param, (cmd, &datafmt, &datevar, 0, 0));

	if (useNames)
		strcpy(datafmt.name, "@floatval");
	else
		datafmt.name[0] = 0;
	datafmt.namelen = CS_NULLTERM;
	datafmt.datatype = CS_FLOAT_TYPE;
	datafmt.status = CS_INPUTVALUE;

	check_call(ct_param, (cmd, &datafmt, &floatvar, 0, 0));

	/*
	 * Send the command to the server
	 */
	check_call(ct_send, (cmd));

	/*
	 * Process the results.
	 */
	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			{
				CS_INT rowsAffected=0;
				ct_res_info(cmd, CS_ROW_COUNT, &rowsAffected, CS_UNUSED, NULL);
				if (1 != rowsAffected) 
					fprintf(stderr, "insert touched %d rows instead of 1\n", rowsAffected);
				else
					rows_found = 1;
			}
			break;

		case CS_CMD_FAIL:
			/*
			 * The server encountered an error while
			 * processing our command.
			 */
			fprintf(stderr, "ct_results returned CS_CMD_FAIL.\n");
			break;

		case CS_STATUS_RESULT:
			/*
			 * The server encountered an error while
			 * processing our command.
			 */
			fprintf(stderr, "ct_results returned CS_STATUS_RESULT.\n");
			break;

		default:
			/*
			 * We got something unexpected.
			 */
			fprintf(stderr, "ct_results returned unexpected result type %d\n", res_type);
			return 1;
		}
	}
	if (ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results returned unexpected result %d.\n", (int) ret);
		exit(1);
	}

	if (!rows_found) {
		fprintf(stderr, "expected 1 rows inserted, inserted %d.\n", (int) rowsAffected);
		exit(1);
	}

	/* test row inserted */
	check_call(run_command, (cmd, "if not exists("
		   "select * from #ctparam_lang where name = 'joe blow' and name2 is not null and age = 2"
		   ") select 1"));

	return 0;
}
