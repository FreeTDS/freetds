#include "common.h"

/*
 * Try to make core dump using SQLBindParameter
 */

static char software_version[] = "$Id: paramcore.c,v 1.6 2008-11-04 14:46:17 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define SP_TEXT "{call sp_paramcore_test(?)}"
#define OUTSTRING_LEN 20

static char MessageText[1000];
static SQLSMALLINT TextLength;
static SQLCHAR SqlState[6];


static void
ReadError(void)
{
	if (SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, SqlState, NULL, (SQLCHAR*) MessageText, sizeof(MessageText), &TextLength))) {
		SqlState[sizeof(SqlState) - 1] = 0;
		fprintf(stderr, "State %s Message: %s\n", SqlState, MessageText);
	}
}



int
main(int argc, char *argv[])
{
	SQLLEN cb = SQL_NTS;

	use_odbc_version3 = 1;

	Connect();

	CommandWithResult(Statement, "drop proc sp_paramcore_test");
	Command("create proc sp_paramcore_test @s varchar(100) output as select @s = '12345'");

	/* here we pass a NULL buffer for input SQL_NTS */
	SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, OUTSTRING_LEN, 0, NULL, OUTSTRING_LEN, &cb);
	ReadError();

	cb = SQL_NTS;
	CommandWithResult(Statement, SP_TEXT);
	ReadError();
	ResetStatement();

	/* here we pass a NULL buffer for input */
	SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_VARCHAR, 18, 0, NULL, OUTSTRING_LEN, &cb);
	ReadError();

	cb = 1;
	CommandWithResult(Statement, SP_TEXT);
	ReadError();
	ResetStatement();

	Command("drop proc sp_paramcore_test");
	Command("create proc sp_paramcore_test @s numeric(10,2) output as select @s = 12345.6");
	ResetStatement();

#if 0	/* this fails even on native platforms */
	/* here we pass a NULL buffer for output */
	cb = sizeof(SQL_NUMERIC_STRUCT);
	SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, SQL_C_NUMERIC, SQL_NUMERIC, 18, 0, NULL, OUTSTRING_LEN, &cb);
	ReadError();

	cb = 1;
	CommandWithResult(Statement, SP_TEXT);
	ReadError();
	ResetStatement();
#endif

	Command("drop proc sp_paramcore_test");

	Disconnect();

	printf("Done successfully!\n");
	return 0;
}
