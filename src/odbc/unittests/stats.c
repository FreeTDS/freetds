#include "common.h"

static char software_version[] = "$Id: stats.c,v 1.1.2.2 2009-12-22 20:16:01 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static SQLLEN cnamesize;
static char output[256];

static void
ReadCol(int i)
{
	memset(output, 'x', sizeof(output));
	strcpy(output, "NULL");
	if (SQLGetData(Statement, i, SQL_C_CHAR, output, sizeof(output), &cnamesize) != SQL_SUCCESS)
		CheckReturn();
}

static const char *catalog = NULL;
static const char *schema = NULL;
static const char *proc = "stat_proc";
static const char *table = "stat_proc";
static const char *column = "@t";

#define LEN(x) (x) ? strlen(x) : SQL_NULL_DATA

static void
TestProc(const char *type, const char *expected)
{
	char sql[256];

	Command(Statement, "IF OBJECT_ID('stat_proc') IS NOT NULL DROP PROC stat_proc");

	sprintf(sql, "CREATE PROC stat_proc(@t %s) AS RETURN 0", type);
	Command(Statement, sql);

	column = "@t";
	if (!SQL_SUCCEEDED(SQLProcedureColumns(Statement, (SQLCHAR *) catalog, LEN(catalog), (SQLCHAR *) schema, LEN(schema), (SQLCHAR *) proc, LEN(proc), (SQLCHAR *) column, LEN(column))))
		CheckReturn();

	if (!SQL_SUCCEEDED(SQLFetch(Statement)))
		CheckReturn();

	ReadCol(6);
	if (strcmp(output, expected) != 0) {
		fprintf(stderr, "Got \"%s\" expected \"%s\"\n", output, expected);
		Disconnect();
		exit(1);
	}

	if (!SQL_SUCCEEDED(SQLCloseCursor(Statement)))
		CheckReturn();
}

static void
TestTable(const char *type, const char *expected)
{
	char sql[256];

	Command(Statement, "IF OBJECT_ID('stat_t') IS NOT NULL DROP TABLE stat_t");

	sprintf(sql, "CREATE TABLE stat_t(t %s)", type);
	Command(Statement, sql);

	column = "t";
	table = "stat_t";
	if (!SQL_SUCCEEDED(SQLColumns(Statement, (SQLCHAR *) catalog, LEN(catalog), (SQLCHAR *) schema, LEN(schema), (SQLCHAR *) table, LEN(table), (SQLCHAR *) column, LEN(column))))
		CheckReturn();

	if (!SQL_SUCCEEDED(SQLFetch(Statement)))
		CheckReturn();

	ReadCol(5);
	if (strcmp(output, expected) != 0) {
		fprintf(stderr, "Got \"%s\" expected \"%s\"\n", output, expected);
		Disconnect();
		exit(1);
	}

	if (!SQL_SUCCEEDED(SQLCloseCursor(Statement)))
		CheckReturn();
}


#define STR(n) str(int_buf, n)

static const char *
str(char *buf, int n)
{
	sprintf(buf, "%d", n);
	return buf;
}

int
main(int argc, char *argv[])
{
	char int_buf[32];

	use_odbc_version3 = 0;
	Connect();

	TestProc("DATETIME", STR(SQL_TIMESTAMP));
	TestTable("DATETIME", STR(SQL_TIMESTAMP));

	Disconnect();


	use_odbc_version3 = 1;
	Connect();

	TestProc("DATETIME", STR(SQL_TYPE_TIMESTAMP));
	TestTable("DATETIME", STR(SQL_TYPE_TIMESTAMP));

	Disconnect();

	printf("Done.\n");
	return 0;
}
