#include "common.h"
#include <odbcss.h>

/* Test binding and calling of TVPs */

#define MAX_ROWS 5
#define MAX_STRING_LENGTH 20

static SQLCHAR buffer[MAX_STRING_LENGTH];

static SQLINTEGER intCol[MAX_ROWS];
static SQLCHAR strCol[MAX_ROWS][MAX_STRING_LENGTH], binCol[MAX_ROWS][MAX_STRING_LENGTH];
static SQL_DATE_STRUCT dateCol[MAX_ROWS];
static SQL_NUMERIC_STRUCT numericCol[MAX_ROWS];

static SQLLEN lIntCol[MAX_ROWS];
static SQLLEN lStrCol[MAX_ROWS];
static SQLLEN lDateCol[MAX_ROWS];
static SQLLEN lNumericCol[MAX_ROWS];
static SQLLEN lBinCol[MAX_ROWS];

static SQLCHAR outputBuffer[256];
static SQLLEN lenBuffer;

/*
 * Generate some data as the columns of our TVPs
 */
static void
setup(void)
{
	int i;

	for (i = 0; i < MAX_ROWS; i++) {
		/* Setup integer column */
		intCol[i] = i * 10;
		lIntCol[i] = sizeof(SQLINTEGER);

		/* Setup string column */
		sprintf((char *) strCol[i], "Dummy value %d", i * 3);
		lStrCol[i] = strlen((char *) strCol[i]);

		/* Setup date column */
		dateCol[i].day = (i % 28) + 1;
		dateCol[i].month = (i * 5) % 12 + 1;
		dateCol[i].year = i + 2000;
		lDateCol[i] = sizeof(SQL_DATE_STRUCT);

		/* Setup numeric values column */
		numericCol[i].precision = 10;
		numericCol[i].scale = 5;
		numericCol[i].sign = i % 2;
		memset(numericCol[i].val, 0, SQL_MAX_NUMERIC_LEN);
		sprintf((char *) numericCol[i].val, "%x", i * 1000);
		lNumericCol[i] = sizeof(SQL_NUMERIC_STRUCT);

		/* Setup binary values column */
		sprintf((char *) binCol[i], "%d", i * 11);
		lBinCol[i] = strlen((char *) binCol[i]);
	}
}

/*
 * Test calling a RPC with a TVP containing 3 columns and 5 rows
 */
static void
TestTVPInsert(void)
{
	SQLCHAR tableName[MAX_STRING_LENGTH];
	SQLLEN numRows;

	strncpy((char *) tableName, "TVPType", MAX_STRING_LENGTH);

	odbc_command("IF OBJECT_ID('TestTVPProc') IS NOT NULL DROP PROC TestTVPProc");
	odbc_command("IF TYPE_ID('TVPType') IS NOT NULL DROP TYPE TVPType");
	odbc_command("IF OBJECT_ID('TVPTable') IS NOT NULL DROP TABLE TVPTable");

	odbc_command("CREATE TABLE TVPTable (PersonID INT PRIMARY KEY, Name VARCHAR(50))");
	odbc_command("CREATE TYPE TVPType " "AS TABLE (vPersonID INT PRIMARY KEY, vName VARCHAR(50))");
	odbc_command("CREATE PROCEDURE TestTVPProc (@TVPParam TVPType READONLY) "
		"AS INSERT INTO TVPTable SELECT * FROM @TVPParam");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 1, SQL_IS_INTEGER, "S");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, intCol, sizeof(SQLINTEGER), lIntCol, "S");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, MAX_STRING_LENGTH, 0, strCol, MAX_STRING_LENGTH, lStrCol, "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 0, SQL_IS_INTEGER, "S");

	/* Population of the StrLen_or_IndPtr buffer can be deferred */
	numRows = MAX_ROWS;

	CHKExecDirect(T("{CALL TestTVPProc(?)}"), SQL_NTS, "S");

	/* Ensure that we successfully add 5 rows */
	odbc_command("SELECT COUNT(*) FROM TVPTable");

	CHKFetch("SI");

	CHKGetData(1, SQL_C_CHAR, outputBuffer, sizeof(outputBuffer), &lenBuffer, "S");
	if (strcmp((char *) outputBuffer, "5") != 0) {
		fprintf(stderr, "Wrong number of columns inserted, expected %ld, got %s\n", (long) numRows, outputBuffer);
		exit(1);
	}

	CHKFetch("No");

	odbc_command("DROP PROC TestTVPProc");
	odbc_command("DROP TYPE TVPType");
	odbc_command("DROP TABLE TVPTable");

	CHKFreeStmt(SQL_RESET_PARAMS, "S");
	CHKCloseCursor("SI");
}

/*
 * Test TVP usage with more parameter types, using a TVP of 2 columns and 5 rows
 */
static void
TestTVPInsert2(void)
{
	SQLCHAR tableName[MAX_STRING_LENGTH];	/* Test explicit schema declaration */
	SQLLEN numRows;

	strncpy((char *) tableName, "dbo.TVPType2", MAX_STRING_LENGTH);

	odbc_command("IF OBJECT_ID('TestTVPProc2') IS NOT NULL DROP PROC TestTVPProc2");
	odbc_command("IF TYPE_ID('TVPType2') IS NOT NULL DROP TYPE TVPType2");
	odbc_command("IF OBJECT_ID('TVPTable2') IS NOT NULL DROP TABLE TVPTable2");

	odbc_command("CREATE TABLE TVPTable2 (Num NUMERIC(10, 5), Bin BINARY(10))");
	odbc_command("CREATE TYPE TVPType2 " "AS TABLE (vNum NUMERIC(10, 5), vBin VARBINARY(10))");
	odbc_command("CREATE PROCEDURE TestTVPProc2 (@TVPParam TVPType2 READONLY) "
		"AS INSERT INTO TVPTable2 SELECT vNum, vBin FROM @TVPParam");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 1, SQL_IS_INTEGER, "S");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_NUMERIC, SQL_NUMERIC,
		10, 5, numericCol, sizeof(SQL_NUMERIC_STRUCT), lNumericCol, "S");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_BINARY,
		MAX_STRING_LENGTH, 4, binCol, MAX_STRING_LENGTH, lBinCol, "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 0, SQL_IS_INTEGER, "S");

	/* Population of the StrLen_or_IndPtr buffer can be deferred */
	numRows = MAX_ROWS;

	CHKExecDirect(T("{CALL TestTVPProc2(?)}"), SQL_NTS, "S");

	/* Ensure that we successfully add 5 rows */
	odbc_command("SELECT COUNT(*) FROM TVPTable2");

	CHKFetch("SI");

	CHKGetData(1, SQL_C_CHAR, outputBuffer, sizeof(outputBuffer), &lenBuffer, "S");
	if (strcmp((char *) outputBuffer, "5") != 0) {
		fprintf(stderr, "Wrong number of columns inserted, expected %ld, got %s\n", (long) numRows, outputBuffer);
		exit(1);
	}

	CHKFetch("No");

	odbc_command("DROP PROC TestTVPProc2");
	odbc_command("DROP TYPE TVPType2");
	odbc_command("DROP TABLE TVPTable2");

	CHKFreeStmt(SQL_RESET_PARAMS, "S");
	CHKCloseCursor("SI");
}

/*
 * Test freeing a TVP without executing it - simulates the case
 * where we encounter an error before calling SQLExecDirect
 */
static void
TestTVPMemoryManagement(void)
{
	SQLCHAR tableName[MAX_STRING_LENGTH];
	SQLLEN numRows;

	strncpy((char *) tableName, "TVPType", MAX_STRING_LENGTH);

	odbc_command("IF OBJECT_ID('TestTVPProc') IS NOT NULL DROP PROC TestTVPProc");
	odbc_command("IF TYPE_ID('TVPType') IS NOT NULL DROP TYPE TVPType");
	odbc_command("IF OBJECT_ID('TVPTable') IS NOT NULL DROP TABLE TVPTable");

	odbc_command("CREATE TABLE TVPTable (PersonID INT PRIMARY KEY, Name VARCHAR(50))");
	odbc_command("CREATE TYPE TVPType " "AS TABLE (vPersonID INT PRIMARY KEY, vName VARCHAR(50))");
	odbc_command("CREATE PROCEDURE TestTVPProc (@TVPParam TVPType READONLY) "
		"AS INSERT INTO TVPTable SELECT * FROM @TVPParam");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 1, SQL_IS_INTEGER, "S");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, intCol, sizeof(SQLINTEGER), lIntCol, "S");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, MAX_STRING_LENGTH, 0, strCol, MAX_STRING_LENGTH, lStrCol, "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 0, SQL_IS_INTEGER, "S");

	odbc_command("DROP PROC TestTVPProc");
	odbc_command("DROP TYPE TVPType");
	odbc_command("DROP TABLE TVPTable");

	CHKFreeStmt(SQL_RESET_PARAMS, "S");
	CHKCloseCursor("SI");
}

int
main(int argc, char *argv[])
{
	setup();

	odbc_connect();

	if (odbc_tds_version() >= 0x703) {
		TestTVPInsert();
		TestTVPInsert2();
		TestTVPMemoryManagement();
	}

	odbc_disconnect();

	return 0;
}
