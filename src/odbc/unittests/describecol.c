#include "common.h"

/*
 * SQLDescribeCol test for precision
 * test what say SQLDescribeCol about precision using some type
 */

static char software_version[] = "$Id: describecol.c,v 1.3 2006-04-15 07:03:39 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int g_result = 0;

static void
DoTest(const char *test_in)
{
	SQLCHAR output[16];
	char sql[256];
	char test_buf[256];
	char *type, *val, *p;

	SQLSMALLINT colType;
	SQLULEN colSize;
	SQLSMALLINT colScale, colNullable;
	SQLINTEGER iColLen, iDescLen, iColPrec, iDescPrec;
	int precision, n;

	strcpy(test_buf, test_in);
	type = strtok(test_buf, " \t");
	val = strtok(NULL, " \t");
	precision = atoi(strtok(NULL, " \t"));

	sprintf(sql, "SELECT CAST(%s AS %s) AS col", val, type);
	/* ignore error, we only need precision of known types */
	if (CommandWithResult(Statement, sql) != SQL_SUCCESS) {
		ResetStatement();
		return;
	}

	if (!SQL_SUCCEEDED(SQLFetch(Statement)))
		ODBC_REPORT_ERROR("Unable to fetch row");

	colSize = 0xbeef;
	if (SQLDescribeCol(Statement, 1, output, sizeof(output), NULL, &colType, &colSize, &colScale, &colNullable) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Error getting data");

	if (SQLColAttribute(Statement, 1, SQL_COLUMN_LENGTH, NULL, SQL_IS_INTEGER, NULL, &iColLen) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");
	if (SQLColAttribute(Statement, 1, SQL_DESC_LENGTH, NULL, SQL_IS_INTEGER, NULL, &iDescLen) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");
	if (SQLColAttribute(Statement, 1, SQL_COLUMN_PRECISION, NULL, SQL_IS_INTEGER, NULL, &iColPrec) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");
	if (SQLColAttribute(Statement, 1, SQL_DESC_PRECISION, NULL, SQL_IS_INTEGER, NULL, &iDescPrec) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");
	printf(" %s prec1 %d len1 %d len2 %d prec2 %d prec3 %d\n", type,
	       (int) colSize, (int) iColLen, (int) iDescLen, (int) iColPrec, (int) iDescPrec);

	if (colSize != iDescLen) {
		g_result = 1;
		fprintf(stderr, "SQLDescribeCol(PRECISION) not the same as SQLDescribeCol(SQL_DESC_LENGTH)\n");
	}

	if (colSize != precision) {
		g_result = 1;
		fprintf(stderr, "Expected precision %d got %d\n", precision, (int) colSize);
	}

	n = precision;
	if ((p = strtok(NULL, " \t")) != NULL)
		n = atoi(p);

	if (n != iColLen) {
		g_result = 1;
		fprintf(stderr, "Expected colLen %d got %d\n", n, (int) iColLen);
	}

	n = precision;
	if ((p = strtok(NULL, " \t")) != NULL)
		n = atoi(p);

	if (n != iColPrec) {
		g_result = 1;
		fprintf(stderr, "Expected colPrec %d got %d\n", n, (int) iColPrec);
	}

	n = precision;
	if ((p = strtok(NULL, " \t")) != NULL)
		n = atoi(p);

	if (n != iDescPrec) {
		g_result = 1;
		fprintf(stderr, "Expected descPrec %d got %d\n", n, (int) iDescPrec);
	}

	SQLMoreResults(Statement);
	ResetStatement();
}

static void
AllTypes(void)
{
	Connect();

	Command(Statement, "SET TEXTSIZE 4096");

	DoTest("bit      0 1");
	DoTest("tinyint  0 3  1");
	DoTest("smallint 0 5  2");
	DoTest("int      0 10 4");
	DoTest("bigint   0 19 8");

	if (use_odbc_version3) {
		DoTest("float 0 53 8 15");
		DoTest("real  0 24 4 7");
	} else {
		DoTest("float 0 15 8");
		DoTest("real  0 7  4");
	}

	DoTest("money      0 19 21");
	DoTest("smallmoney 0 10 12");

	DoTest("numeric(10,2) 0 10 12");
	DoTest("numeric(23,4) 0 23 25");

	DoTest("datetime      '2006-04-14' 23 16 23 3");
	DoTest("smalldatetime '2006-04-14' 16 16 16 0");

	DoTest("char(10)     'hi!' 10");
	DoTest("varchar(11)  'hi!' 11");
	DoTest("nchar(12)    'hi!' 12   24");
	DoTest("nvarchar(13) 'hi!' 13   26");
	DoTest("text         'hi!' 4096");
	DoTest("ntext        'hi!' 2048 4096");

	DoTest("binary(10)    'hi!' 10");
	DoTest("varbinary(11) 'hi!' 11");
	DoTest("image         'hi!' 4096");

	Disconnect();
}

int
main(int argc, char *argv[])
{
	use_odbc_version3 = 0;
	AllTypes();

	use_odbc_version3 = 1;
	AllTypes();

	printf("Done.\n");
	return g_result;
}
