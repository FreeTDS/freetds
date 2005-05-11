#include "common.h"

/* Test for {?=call store(?,123,'foo')} syntax and run */

static char software_version[] = "$Id: const_params.c,v 1.7 2005-05-11 19:52:25 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLINTEGER input, ind, ind2, ind3, output;
	SQLINTEGER out1;

	Connect();

	if (CommandWithResult(Statement, "drop proc const_param") != SQL_SUCCESS)
		printf("Unable to execute statement\n");

	Command(Statement, "create proc const_param @in1 int, @in2 int, @in3 datetime, @in4 varchar(10), @out int output as\n"
		"begin\n"
		" set nocount on\n"
		" select @out = 7654321\n"
		" if @in1 <> @in2 or @in3 <> convert(datetime, '2004-10-15 12:09:08') or @in4 <> 'foo'\n"
		"  select @out = 1234567\n"
		" return 24680\n"
		"end");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &input, 0, &ind) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind input parameter");

	if (SQLBindParameter(Statement, 2, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &out1, 0, &ind2) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind output parameter");

	/* TODO use {ts ...} for date */
	if (SQLPrepare(Statement, (SQLCHAR *) "{call const_param(?, 13579, '2004-10-15 12:09:08', 'foo', ?)}", SQL_NTS) !=
	    SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to prepare statement");

	input = 13579;
	ind = sizeof(input);
	out1 = output = 0xdeadbeef;
	if (SQLExecute(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to execute statement");

	if (out1 != 7654321) {
		fprintf(stderr, "Invalid output %d (0x%x)\n", (int) out1, (int) out1);
		exit(1);
	}

	/* just to reset some possible buffers */
	Command(Statement, "DECLARE @i INT");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &output, 0, &ind) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind output parameter");

	if (SQLBindParameter(Statement, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &input, 0, &ind2) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind input parameter");

	if (SQLBindParameter(Statement, 3, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &out1, 0, &ind3) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to bind output parameter");

	/* TODO use {ts ...} for date */
	if (SQLPrepare(Statement, (SQLCHAR *) "{?=call const_param(?, , '2004-10-15 12:09:08', 'foo', ?)}", SQL_NTS) !=
	    SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to prepare statement");

	input = 13579;
	ind2 = sizeof(input);
	out1 = output = 0xdeadbeef;
	if (SQLExecute(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to execute statement");

	if (out1 != 7654321) {
		fprintf(stderr, "Invalid output %d (0x%x)\n", (int) out1, (int) out1);
		exit(1);
	}

	if (output != 24680) {
		fprintf(stderr, "Invalid result %d (0x%x)\n", (int) output, (int) output);
		exit(1);
	}

	if (CommandWithResult(Statement, "drop proc const_param") != SQL_SUCCESS)
		printf("Unable to execute statement\n");

	Disconnect();

	printf("Done.\n");
	return 0;
}
