#include "common.h"

/*
 * SQLDescribeCol test for precision
 * test what say SQLDescribeCol about precision using some type
 */

static char software_version[] = "$Id: describecol.c,v 1.6 2006-06-20 12:33:59 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int g_result = 0;
static const char *curr_test = NULL;

static void
check_attr(const char *what, int got)
{
	int n;
	char *p = strtok(NULL, " \t");

	if (!p) {
		fprintf(stderr, "Int not found parsing test '%s'\n", curr_test);
		exit(1);
	}

	n = atoi(p);

	if (got != n) {
		g_result = 1;
		fprintf(stderr, "Expected SQLColAttribute(%s) %d got %d\n", what, n, got);
	}
}

static void
DoTest(const char *test_in)
{
	SQLCHAR output[16];
	char sql[256];
	char test_buf[256];
	char *type, *val;

	SQLSMALLINT colType;
	SQLULEN colSize;
	SQLSMALLINT colScale, colNullable;
	SQLINTEGER iColLen, iColPrec, iColScale;
	SQLINTEGER iDescLen, iDescPrec, iScale, iOctetLen, iDisplaySize;

	curr_test = test_in;

	strcpy(test_buf, test_in);
	type = strtok(test_buf, " \t");
	val = strtok(NULL, " \t");

	sprintf(sql, "SELECT CONVERT(%s, %s) AS col", type, val);
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

	iColLen = iColPrec = iColScale = 0xdeadbeef;
	if (SQLColAttribute(Statement, 1, SQL_COLUMN_LENGTH, NULL, SQL_IS_INTEGER, NULL, &iColLen) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");
	if (SQLColAttribute(Statement, 1, SQL_COLUMN_PRECISION, NULL, SQL_IS_INTEGER, NULL, &iColPrec) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");
	if (SQLColAttribute(Statement, 1, SQL_COLUMN_SCALE, NULL, SQL_IS_INTEGER, NULL, &iColScale) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");

	iDescLen = iDescPrec = iScale = iOctetLen = iDisplaySize = 0xdeadbeef;
	if (SQLColAttribute(Statement, 1, SQL_DESC_LENGTH, NULL, SQL_IS_INTEGER, NULL, &iDescLen) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");
	if (SQLColAttribute(Statement, 1, SQL_DESC_OCTET_LENGTH, NULL, SQL_IS_INTEGER, NULL, &iOctetLen) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");
	if (SQLColAttribute(Statement, 1, SQL_DESC_PRECISION, NULL, SQL_IS_INTEGER, NULL, &iDescPrec) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");
	if (SQLColAttribute(Statement, 1, SQL_DESC_SCALE, NULL, SQL_IS_INTEGER, NULL, &iScale) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");
	if (SQLColAttribute(Statement, 1, SQL_DESC_DISPLAY_SIZE, NULL, SQL_IS_INTEGER, NULL, &iDisplaySize) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("SQLColAttribute");

	printf(" %s prec %d\n"
	       "\tCOLUMN_LENGTH %d COLUMN_PRECISION %d COLUMN_SCALE %d\n"
	       "\tDESC_LENGTH %d DESC_OCTET_LENGTH %d DESC_PRECISION %d DESC_SCALE %d\n",
	       type, (int) colSize,
	       (int) iColLen, (int) iColPrec, (int) iColScale,
	       (int) iDescLen, (int) iOctetLen, (int) iDescPrec, (int) iScale);

	if (colSize != iDescLen) {
		g_result = 1;
		fprintf(stderr, "SQLDescribeCol(PRECISION) not the same as SQLColAttribute(SQL_DESC_LENGTH)\n");
	}

	check_attr("COLUMN_LENGTH", iColLen);
	check_attr("COLUMN_PRECISION", iColPrec);
	check_attr("COLUMN_SCALE", iColScale);
	check_attr("DESC_LENGTH", iDescLen);
	check_attr("DESC_OCTET_LENGTH", iOctetLen);
	check_attr("DESC_PRECISION", iDescPrec);
	check_attr("DESC_SCALE", iScale);
	check_attr("DESC_DISPLAY_SIZE", iDisplaySize);

	SQLMoreResults(Statement);
	ResetStatement();
}

static void
AllTypes(void)
{
	Connect();

	Command(Statement, "SET TEXTSIZE 4096");

	DoTest("bit      0  1  1 0  1 1  1 0  1");
	DoTest("tinyint  0  1  3 0  3 1  3 0  3");
	DoTest("smallint 0  2  5 0  5 2  5 0  6");
	DoTest("int      0  4 10 0 10 4 10 0 11");
	DoTest("bigint   0  8 19 0 19 8 19 0 20");

	if (use_odbc_version3) {
		DoTest("real  0 4  7 0 24 4 24 0 14");
		DoTest("float 0 8 15 0 53 8 53 0 24");
	} else {
		DoTest("real  0 4  7 0  7 4  7 0 14");
		DoTest("float 0 8 15 0 15 8 15 0 24");
	}

	DoTest("smallmoney 0 12 10 4 10 12 10 4 12");
	DoTest("money      0 21 19 4 19 21 19 4 21");

	DoTest("numeric(10,2) 0 12 10 2 10 12 10 2 12");
	DoTest("numeric(23,4) 0 25 23 4 23 25 23 4 25");

	DoTest("datetime      '2006-04-14' 16 23 3 23 16 3 3 23");
	DoTest("smalldatetime '2006-04-14' 16 16 0 16 16 0 0 19");

	DoTest("char(10)     'hi!'   10   10 0   10   10   10 0   10");
	DoTest("varchar(11)  'hi!'   11   11 0   11   11   11 0   11");
	DoTest("nchar(12)    'hi!'   24   12 0   12   24   12 0   12");
	DoTest("nvarchar(13) 'hi!'   26   13 0   13   26   13 0   13");
	DoTest("text         'hi!' 4096 4096 0 4096 4096 4096 0 4096");
	DoTest("ntext        'hi!' 4096 2048 0 2048 4096 2048 0 2048");

	DoTest("binary(10)    'hi!'   10   10 0   10   10   10 0   20");
	DoTest("varbinary(11) 'hi!'   11   11 0   11   11   11 0   22");
	DoTest("image         'hi!' 4096 4096 0 4096 4096 4096 0 8192");

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
