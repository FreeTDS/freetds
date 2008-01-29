#include "common.h"

/* Test for {?=call store(?,123,'foo')} syntax and run */

static char software_version[] = "$Id: const_params.c,v 1.14 2008-01-29 14:30:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	SQLINTEGER input, output;
	SQLINTEGER out1;
	SQLLEN ind, ind2, ind3;

	Connect();

	if (CommandWithResult(Statement, "drop proc const_param") != SQL_SUCCESS)
		printf("Unable to execute statement\n");

	Command(Statement, "create proc const_param @in1 int, @in2 int, @in3 datetime, @in4 varchar(10), @out int output as\n"
		"begin\n"
		" set nocount on\n"
		" select @out = 7654321\n"
		" if (@in1 <> @in2 and @in2 is not null) or @in3 <> convert(datetime, '2004-10-15 12:09:08') or @in4 <> 'foo'\n"
		"  select @out = 1234567\n"
		" return 24680\n"
		"end");

	CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &input, 0, &ind));

	CHK(SQLBindParameter, (Statement, 2, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &out1, 0, &ind2));

	/* TODO use {ts ...} for date */
	CHK(SQLPrepare, (Statement, (SQLCHAR *) "{call const_param(?, 13579, '2004-10-15 12:09:08', 'foo', ?)}", SQL_NTS));

	input = 13579;
	ind = sizeof(input);
	out1 = output = 0xdeadbeef;
	CHK(SQLExecute, (Statement));

	if (out1 != 7654321) {
		fprintf(stderr, "Invalid output %d (0x%x)\n", (int) out1, (int) out1);
		exit(1);
	}

	/* just to reset some possible buffers */
	Command(Statement, "DECLARE @i INT");

	CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &output, 0, &ind));
	CHK(SQLBindParameter, (Statement, 2, SQL_PARAM_INPUT,  SQL_C_SLONG, SQL_INTEGER, 0, 0, &input,  0, &ind2));
	CHK(SQLBindParameter, (Statement, 3, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &out1,   0, &ind3));

	/* TODO use {ts ...} for date */
	CHK(SQLPrepare, (Statement, (SQLCHAR *) "{?=call const_param(?, , '2004-10-15 12:09:08', 'foo', ?)}", SQL_NTS));

	input = 13579;
	ind2 = sizeof(input);
	out1 = output = 0xdeadbeef;
	CHK(SQLExecute, (Statement));

	if (out1 != 7654321) {
		fprintf(stderr, "Invalid output %d (0x%x)\n", (int) out1, (int) out1);
		exit(1);
	}

	if (output != 24680) {
		fprintf(stderr, "Invalid result %d (0x%x) expected 24680\n", (int) output, (int) output);
		exit(1);
	}

	Command(Statement, "IF OBJECT_ID('const_param') IS NOT NULL DROP PROC const_param");

	Command(Statement, "create proc const_param @in1 float, @in2 varbinary(100) as\n"
		"begin\n"
		" if @in1 <> 12.5 or @in2 <> 0x0102030405060708\n"
		"  return 12345\n"
		" return 54321\n"
		"end");

	CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_OUTPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &output, 0, &ind));

	CHK(SQLPrepare, (Statement, (SQLCHAR *) "{?=call const_param(12.5, 0x0102030405060708)}", SQL_NTS));

	output = 0xdeadbeef;
	CHK(SQLExecute, (Statement));

	if (output != 54321) {
		fprintf(stderr, "Invalid result %d (0x%x) expected 54321\n", (int) output, (int) output);
		return 1;
	}

	Command(Statement, "drop proc const_param");

	Command(Statement, "create proc const_param @in varchar(20) as\n"
		"begin\n"
		" if @in = 'value' select 8421\n"
		" select 1248\n"
		"end");

	/* catch problem reported by Peter Deacon */
	output = 0xdeadbeef;
	Command(Statement, "{CALL const_param('value')}");
	CHK(SQLBindCol, (Statement, 1, SQL_C_SLONG, &output, 0, &ind));
	SQLFetch(Statement);

	if (output != 8421) {
		fprintf(stderr, "Invalid result %d (0x%x)\n", (int) output, (int) output);
		return 1;
	}

	ResetStatement();

	Command(Statement, "drop proc const_param");

	Disconnect();

	printf("Done.\n");
	return 0;
}
