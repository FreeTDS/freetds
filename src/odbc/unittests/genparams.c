#include "common.h"

/* Test various type from odbc and to odbc */

static char software_version[] = "$Id: genparams.c,v 1.2 2003-11-13 13:52:53 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void
Test(const char *type, const char *value_to_convert, SQLSMALLINT out_c_type, SQLSMALLINT out_sql_type, const char *expected)
{
	char sbuf[1024];
	unsigned char out_buf[256];
	SQLINTEGER out_len = 0;
	SQL_NUMERIC_STRUCT *num;
	int i;

	/* build store procedure to test */
	sprintf(sbuf, "CREATE PROC spTestProc @i %s OUTPUT AS SELECT @i = CONVERT(%s, '%s')", type, type, value_to_convert);
	Command(Statement, sbuf);

	/* bind parameter */
	if (SQLBindParameter(Statement, 1, SQL_PARAM_OUTPUT, out_c_type, out_sql_type, 18, 0, out_buf, sizeof(out_buf), &out_len) !=
	    SQL_SUCCESS) {
		fprintf(stderr, "Unable to bind input parameter\n");
		CheckReturn();
	}
	
		/* call store procedure */
		if (SQLExecDirect(Statement, "{call spTestProc(?)}", SQL_NTS) != SQL_SUCCESS) {
		fprintf(stderr, "Unable to execute store statement\n");
		CheckReturn();
	}

	/* test results */
	sbuf[0] = 0;
	switch (out_c_type) {
	case SQL_C_NUMERIC:
		num = (SQL_NUMERIC_STRUCT *) out_buf;
		sprintf(sbuf, "%d %d %d ", num->precision, num->scale, num->sign);
		i = SQL_MAX_NUMERIC_LEN;
		for (; i > 0 && !num->val[--i];);
		for (; i >= 0; --i)
			sprintf(strchr(sbuf, 0), "%02X", num->val[i]);
		break;
	}

	if (strcmp(sbuf, expected) != 0) {
		fprintf(stderr, "Wrong result\n  Got: %s\n  Expected: %s\n", sbuf, expected);
		exit(1);
	}
	Command(Statement, "drop proc spTestProc");
}

int
main(int argc, char *argv[])
{
	Connect();

	if (CommandWithResult(Statement, "drop proc spTestProc") != SQL_SUCCESS)
		printf("Unable to execute statement\n");

	/* FIXME why should return 38 0 as precision and scale ?? correct ?? */
	Test("NUMERIC(18,2)", "123", SQL_C_NUMERIC, SQL_NUMERIC, "18 0 1 7B");

	Disconnect();

	printf("Done successfully!\n");
	return 0;
}
