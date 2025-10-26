/* Test binding and calling of TVPs */

#include "common.h"
#include <assert.h>
#include <odbcss.h>

#undef MEMORY_TESTS
#if defined(HAVE_MALLOC_H)
#  include <malloc.h>
#  if defined(HAVE_MALLINFO2) || defined(HAVE_MALLINFO) || defined(HAVE__HEAPWALK)
#    define MEMORY_TESTS 1
#  endif
#endif

#if defined(HAVE_VALGRIND_MEMCHECK_H)
#  include <valgrind/valgrind.h>
#else
#  define RUNNING_ON_VALGRIND 0
#endif

#include <freetds/bool.h>

#define MAX_ROWS 5
#define MAX_STRING_LENGTH 20

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

typedef union {
	SQLPOINTER fldSQLPOINTER;
	SQLSMALLINT fldSQLSMALLINT;
	SQLUSMALLINT fldSQLUSMALLINT;
	SQLINTEGER fldSQLINTEGER;
	SQLUINTEGER fldSQLUINTEGER;
	SQLLEN fldSQLLEN;
	SQLULEN fldSQLULEN;
} field_output;

/* utility to get a field from descriptor */
static field_output
get_desc_field(SQLINTEGER desc_type, SQLSMALLINT icol, SQLSMALLINT fDescType, size_t size)
{
	SQLHDESC desc;
	SQLINTEGER ind;
	field_output buf;

	assert(size <= sizeof(buf));
	CHKGetStmtAttr(desc_type, &desc, sizeof(desc), &ind, "S");

	memset(&buf, 0x5a, sizeof(buf));
	ind = 1234;
	CHKGetDescField(desc, icol, fDescType, &buf, (SQLINTEGER) size, &ind,
			"S");
	assert(ind == size);

	return buf;
}

/* Utility to get a field from descriptor.
 * desc_type   APP or IMP.
 * col         column number (or 0 if does not matter).
 * field       descriptor field (SQL_DESC_xxx).
 * type        SQL type to be returned, strings not supported.
 */
#define GET_DESC_FIELD(desc_type, col, field, type) \
	(get_desc_field(SQL_ATTR_ ## desc_type ## _PARAM_DESC, col, field, sizeof(type)).fld ## type)

/* utility to check condition and returns error string */
static char*
check_cond(bool condition, const char *fmt, ...)
{
	va_list ap;
	char *ret = NULL;

	if (condition)
		return ret;

	va_start(ap, fmt);
	assert(vasprintf(&ret, fmt, ap) >= 0);
	va_end(ap);
	return ret;
}

#define CHECK_COND(args) do { \
	char *err = check_cond args; \
	if (err) { \
		failed = true; \
		fprintf(stderr, "Wrong condition at line %d: %s\n", __LINE__, err); \
		free(err); \
	} \
} while(0)

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
		numericCol[i].scale = 0;
		numericCol[i].sign = i % 2;
		memset(numericCol[i].val, 0, SQL_MAX_NUMERIC_LEN);
		sprintf((char *) numericCol[i].val, "%x", i * 10);
		lNumericCol[i] = sizeof(SQL_NUMERIC_STRUCT);

		/* Setup binary values column */
		sprintf((char *) binCol[i], "%d", i * 11);
		lBinCol[i] = strlen((char *) binCol[i]);
	}
}

static void
dirty_name(SQLWCHAR *name)
{
	const SQLWCHAR f = name[0];
	name[0] = (f == 'X' || f == 'x') ? 'Y' : 'X';
}

/*
 * Test calling a RPC with a TVP containing 3 columns and 4 rows
 */
static void
TestTVPInsert(void)
{
	SQLWCHAR *tableName;
	SQLLEN numRows;
	SQLHDESC apd;
	bool failed = false;
	SQLPOINTER ptr;

	/* here we append some dummy string to check binding with a length */
	tableName = odbc_get_sqlwchar(&odbc_buf, "TVPType_and_garbage");

	odbc_command("IF OBJECT_ID('TestTVPProc') IS NOT NULL DROP PROC TestTVPProc");
	odbc_command("IF TYPE_ID('TVPType') IS NOT NULL DROP TYPE TVPType");
	odbc_command("IF OBJECT_ID('TVPTable') IS NOT NULL DROP TABLE TVPTable");

	odbc_command("CREATE TABLE TVPTable (PersonID INT PRIMARY KEY, Name VARCHAR(50))");
	odbc_command("CREATE TYPE TVPType " "AS TABLE (vPersonID INT PRIMARY KEY, vName VARCHAR(50))");
	odbc_command("CREATE PROCEDURE TestTVPProc (@TVPParam TVPType READONLY) "
		"AS INSERT INTO TVPTable SELECT * FROM @TVPParam");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, intCol, sizeof(SQLINTEGER), lIntCol, "S");
	ptr = GET_DESC_FIELD(APP, 1, SQL_DESC_DATA_PTR, SQLPOINTER);
	CHECK_COND((ptr == intCol, "SQL_DESC_DATA_PTR expected %p got %p", intCol, ptr));
	CHKGetStmtAttr(SQL_ATTR_APP_PARAM_DESC, &apd, sizeof(apd), NULL, "S");
	CHKSetDescField(apd, 1, SQL_DESC_CONCISE_TYPE, TDS_INT2PTR(SQL_C_DOUBLE), sizeof(SQLSMALLINT), "S");
	ptr = GET_DESC_FIELD(APP, 1, SQL_DESC_DATA_PTR, SQLPOINTER);
	CHECK_COND((ptr == NULL, "SQL_DESC_DATA_PTR expected %p got %p", NULL, ptr));
	assert(!failed);

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, 7 * sizeof(SQLWCHAR), NULL, "S");
	dirty_name(tableName);

	CHKGetStmtAttr(SQL_ATTR_APP_PARAM_DESC, &apd, sizeof(apd), NULL, "S");
	CHKSetDescField(apd, 1, SQL_DESC_OCTET_LENGTH_PTR, (SQLPOINTER) &numRows, sizeof(void*), "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 1, SQL_IS_INTEGER, "S");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, intCol, sizeof(SQLINTEGER), lIntCol, "S");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, MAX_STRING_LENGTH, 0, strCol, MAX_STRING_LENGTH, lStrCol, "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 0, SQL_IS_INTEGER, "S");

	/* Population of the StrLen_or_IndPtr buffer can be deferred */
	/* We use one rows less than the maximum to check if code is using the right value */
	numRows = MAX_ROWS - 1;

	CHKExecDirect(T("{CALL TestTVPProc(?)}"), SQL_NTS, "S");

	/* Ensure that we successfully add 5 rows */
	odbc_command("SELECT COUNT(*) FROM TVPTable");

	CHKFetch("SI");

	CHKGetData(1, SQL_C_CHAR, outputBuffer, sizeof(outputBuffer), &lenBuffer, "S");
	if (atoi((char *) outputBuffer) != numRows) {
		fprintf(stderr, "Wrong number of rows inserted, expected %ld, got %s\n", (long) numRows, outputBuffer);
		exit(1);
	}

	CHKFetch("No");
	CHKCloseCursor("SI");

	/* check all rows are present */
	odbc_check_no_row("IF NOT EXISTS(SELECT * FROM TVPTable WHERE PersonID = 0 AND Name = 'Dummy value 0') SELECT 1");
	odbc_check_no_row("IF NOT EXISTS(SELECT * FROM TVPTable WHERE PersonID = 10 AND Name = 'Dummy value 3') SELECT 1");
	odbc_check_no_row("IF NOT EXISTS(SELECT * FROM TVPTable WHERE PersonID = 20 AND Name = 'Dummy value 6') SELECT 1");
	odbc_check_no_row("IF NOT EXISTS(SELECT * FROM TVPTable WHERE PersonID = 30 AND Name = 'Dummy value 9') SELECT 1");
	odbc_check_no_row("IF EXISTS(SELECT * FROM TVPTable WHERE PersonID = 40 AND Name = 'Dummy value 12') SELECT 1");

	odbc_command("DROP PROC TestTVPProc");
	odbc_command("DROP TYPE TVPType");
	odbc_command("DROP TABLE TVPTable");

	CHKFreeStmt(SQL_RESET_PARAMS, "S");
}

/*
 * Test TVP usage with more parameter types, using a TVP of 2 columns and 5 rows
 */
static void
TestTVPInsert2(void)
{
	SQLWCHAR *tableName;	/* Test explicit schema declaration */
	SQLLEN numRows;

	tableName = odbc_get_sqlwchar(&odbc_buf, "TVPType2");

	odbc_command("IF OBJECT_ID('TestTVPProc2') IS NOT NULL DROP PROC TestTVPProc2");
	odbc_command("IF TYPE_ID('TVPType2') IS NOT NULL DROP TYPE TVPType2");
	odbc_command("IF OBJECT_ID('TVPTable2') IS NOT NULL DROP TABLE TVPTable2");

	odbc_command("CREATE TABLE TVPTable2 (Num NUMERIC(10, 5), Bin BINARY(10))");
	odbc_command("CREATE TYPE TVPType2 " "AS TABLE (vNum NUMERIC(10, 5), vBin VARBINARY(10))");
	odbc_command("CREATE PROCEDURE TestTVPProc2 (@TVPParam TVPType2 READONLY) "
		"AS INSERT INTO TVPTable2 SELECT vNum, vBin FROM @TVPParam");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");
	dirty_name(tableName);

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
	CHKFetch("No");
	CHKCloseCursor("SI");

	odbc_check_no_row("IF NOT EXISTS(SELECT * FROM TVPTable2 WHERE Bin = 0x30 AND Num = -48) SELECT 1");
	odbc_check_no_row("IF NOT EXISTS(SELECT * FROM TVPTable2 WHERE Bin = 0x3131 AND Num = 97) SELECT 1");
	odbc_check_no_row("IF NOT EXISTS(SELECT * FROM TVPTable2 WHERE Bin = 0x3232 AND Num = -13361) SELECT 1");
	odbc_check_no_row("IF NOT EXISTS(SELECT * FROM TVPTable2 WHERE Bin = 0x3333 AND Num = 25905) SELECT 1");
	odbc_check_no_row("IF NOT EXISTS(SELECT * FROM TVPTable2 WHERE Bin = 0x3434 AND Num = -14386) SELECT 1");

	odbc_command("DROP PROC TestTVPProc2");
	odbc_command("DROP TYPE TVPType2");
	odbc_command("DROP TABLE TVPTable2");

	CHKFreeStmt(SQL_RESET_PARAMS, "S");
}

/*
 * Test freeing a TVP without executing it - simulates the case
 * where we encounter an error before calling SQLExecDirect
 */
static void
TestTVPMemoryManagement(void)
{
	SQLWCHAR *tableName;
	SQLLEN numRows;

	tableName = odbc_get_sqlwchar(&odbc_buf, "TVPType");

	odbc_command("IF OBJECT_ID('TestTVPProc') IS NOT NULL DROP PROC TestTVPProc");
	odbc_command("IF TYPE_ID('TVPType') IS NOT NULL DROP TYPE TVPType");
	odbc_command("IF OBJECT_ID('TVPTable') IS NOT NULL DROP TABLE TVPTable");

	odbc_command("CREATE TABLE TVPTable (PersonID INT PRIMARY KEY, Name VARCHAR(50))");
	odbc_command("CREATE TYPE TVPType " "AS TABLE (vPersonID INT PRIMARY KEY, vName VARCHAR(50))");
	odbc_command("CREATE PROCEDURE TestTVPProc (@TVPParam TVPType READONLY) "
		"AS INSERT INTO TVPTable SELECT * FROM @TVPParam");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");
	dirty_name(tableName);

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 1, SQL_IS_INTEGER, "S");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, intCol, sizeof(SQLINTEGER), lIntCol, "S");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, MAX_STRING_LENGTH, 0, strCol, MAX_STRING_LENGTH, lStrCol, "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 0, SQL_IS_INTEGER, "S");

	odbc_command("DROP PROC TestTVPProc");
	odbc_command("DROP TYPE TVPType");
	odbc_command("DROP TABLE TVPTable");

	CHKFreeStmt(SQL_RESET_PARAMS, "S");
}

/* Test some errors happens when we expect them */
static void
TestErrors(void)
{
	SQLWCHAR *tableName;
	SQLLEN numRows;

	tableName = odbc_get_sqlwchar(&odbc_buf, "TVPType");

	/* SQL error 07006 -- [Microsoft][ODBC Driver 17 for SQL Server]Restricted data type attribute violation */
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "E");
	odbc_read_error();
	assert(strcmp(odbc_sqlstate, "07006") == 0);

	/* SQL error HY105 -- [Microsoft][ODBC Driver 17 for SQL Server]Invalid parameter type */
	CHKBindParameter(1, SQL_PARAM_OUTPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "E");
	odbc_read_error();
	assert(strcmp(odbc_sqlstate, "HY105") == 0);

	/* SQL error HY105 -- [Microsoft][ODBC Driver 17 for SQL Server]Invalid parameter type */
	CHKBindParameter(1, SQL_PARAM_OUTPUT, SQL_C_LONG, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "E");
	odbc_read_error();
	assert(strcmp(odbc_sqlstate, "HY105") == 0);

	/* SQL error HY105 -- [Microsoft][ODBC Driver 17 for SQL Server]Invalid parameter type */
	CHKBindParameter(1, SQL_PARAM_INPUT_OUTPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "E");
	odbc_read_error();
	assert(strcmp(odbc_sqlstate, "HY105") == 0);

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");
	tableName[0] = 'A';

	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, intCol, sizeof(SQLINTEGER), lIntCol, "S");

	/* SQL error IM020 -- [Microsoft][ODBC Driver 17 for SQL Server]Parameter focus does not refer to a table-valued parameter */
	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 2, SQL_IS_INTEGER, "E");
	odbc_read_error();
	assert(strcmp(odbc_sqlstate, "IM020") == 0);

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 1, SQL_IS_INTEGER, "S");

	/* SQL error HY004 -- [Microsoft][ODBC Driver 17 for SQL Server]Invalid SQL data type */
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "E");
	odbc_read_error();
	assert(strcmp(odbc_sqlstate, "HY004") == 0);

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, intCol, sizeof(SQLINTEGER), lIntCol, "S");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, MAX_STRING_LENGTH, 0, strCol, MAX_STRING_LENGTH, lStrCol, "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 0, SQL_IS_INTEGER, "S");

	odbc_reset_statement();
}

static void
TestDescriptorValues(void)
{
	SQLWCHAR *tableName;
	SQLLEN numRows;
	SQLPOINTER ptr;
	SQLSMALLINT type;
	SQLLEN count, len;
	bool failed = false;

	tableName = odbc_get_sqlwchar(&odbc_buf, "TVPType");

	count = GET_DESC_FIELD(APP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 0, "count %d == 0", (int) count));
	count = GET_DESC_FIELD(IMP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 0, "count %d == 0", (int) count));

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");
	dirty_name(tableName);

	count = GET_DESC_FIELD(APP, 1, SQL_DESC_LENGTH, SQLULEN);
	CHECK_COND((count == 1, "count %d == 1", (int) count));
	count = GET_DESC_FIELD(IMP, 1, SQL_DESC_LENGTH, SQLULEN);
	CHECK_COND((count == 0, "count %d == 0", (int) count));

	count = GET_DESC_FIELD(APP, 0, SQL_DESC_ARRAY_SIZE, SQLULEN);
	CHECK_COND((count == 1, "count %d == 1", (int) count));

	count = GET_DESC_FIELD(APP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 1, "count %d == 1", (int) count));
	count = GET_DESC_FIELD(IMP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 1, "count %d == 1", (int) count));

	/* data pointer should point to table name */
	ptr = GET_DESC_FIELD(APP, 1, SQL_DESC_DATA_PTR, SQLPOINTER);
	CHECK_COND((ptr == tableName, "SQL_DESC_DATA_PTR expected %p got %p", tableName, ptr));
	if (odbc_driver_is_freetds()) {
		/* MS driver cannot read this, used internally by FreeTDS */
		ptr = GET_DESC_FIELD(IMP, 1, SQL_DESC_DATA_PTR, SQLPOINTER);
		printf("Internal pointer %p\n", ptr);
	}

	/* indicator should be the pointer to number of rows */
	ptr = GET_DESC_FIELD(APP, 1, SQL_DESC_INDICATOR_PTR, SQLPOINTER);
	CHECK_COND((ptr == &numRows, "SQL_DESC_INDICATOR_PTR expected %p got %p", &numRows, ptr));
	if (odbc_driver_is_freetds()) {
		ptr = GET_DESC_FIELD(IMP, 1, SQL_DESC_INDICATOR_PTR, SQLPOINTER);
		CHECK_COND((ptr == NULL, "SQL_DESC_INDICATOR_PTR expected %p got %p", NULL, ptr));
	}

	/* octect length pointer should be the pointer to number of rows */
	ptr = GET_DESC_FIELD(APP, 1, SQL_DESC_OCTET_LENGTH_PTR, SQLPOINTER);
	CHECK_COND((ptr == &numRows, "SQL_DESC_OCTET_LENGTH_PTR expected %p got %p", &numRows, ptr));
	if (odbc_driver_is_freetds()) {
		ptr = GET_DESC_FIELD(IMP, 1, SQL_DESC_OCTET_LENGTH_PTR, SQLPOINTER);
		CHECK_COND((ptr == NULL, "SQL_DESC_OCTET_LENGTH_PTR expected %p got %p", NULL, ptr));
	}

	/* this should be then length of tableName passed to SQLBindParameter */
	len = GET_DESC_FIELD(APP, 1, SQL_DESC_OCTET_LENGTH, SQLLEN);
	CHECK_COND((len == SQL_NTS, "SQL_DESC_OCTET_LENGTH expected %ld got %ld", (long) SQL_NTS, (long) len));
	len = GET_DESC_FIELD(IMP, 1, SQL_DESC_OCTET_LENGTH, SQLLEN);
	CHECK_COND((len == 0, "SQL_DESC_OCTET_LENGTH expected %ld got %ld", (long) 0, (long) len));

	type = GET_DESC_FIELD(APP, 1, SQL_DESC_CONCISE_TYPE, SQLSMALLINT);
	CHECK_COND((type == SQL_C_BINARY, "SQL_DESC_CONCISE_TYPE expected %d got %d", SQL_C_BINARY, type));
	type = GET_DESC_FIELD(IMP, 1, SQL_DESC_CONCISE_TYPE, SQLSMALLINT);
	CHECK_COND((type == SQL_SS_TABLE, "SQL_DESC_CONCISE_TYPE expected %d got %d", SQL_SS_TABLE, type));

	/* setting parameter focus should move to different descriptors */
	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 1, SQL_IS_INTEGER, "S");

	count = GET_DESC_FIELD(APP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 0, "count %d == 0", (int) count));
	count = GET_DESC_FIELD(IMP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 0, "count %d == 0", (int) count));

	count = GET_DESC_FIELD(APP, 0, SQL_DESC_ARRAY_SIZE, SQLULEN);
	CHECK_COND((count == 5, "count %d == 5", (int) count));

	/* modify descriptors */
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, intCol, sizeof(SQLINTEGER), lIntCol, "S");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, MAX_STRING_LENGTH, 0, strCol, MAX_STRING_LENGTH, lStrCol, "S");

	count = GET_DESC_FIELD(APP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 2, "count %d == 2", (int) count));
	count = GET_DESC_FIELD(IMP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 2, "count %d == 2", (int) count));

	/* switch back to main descriptors */
	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 0, SQL_IS_INTEGER, "S");

	count = GET_DESC_FIELD(APP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 1, "count %d == 1", (int) count));
	count = GET_DESC_FIELD(IMP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 1, "count %d == 1", (int) count));

	/* switch back to table */
	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 1, SQL_IS_INTEGER, "S");

	/* this should fail, cannot set table inside a table */
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "E");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 0, SQL_IS_INTEGER, "S");

	/* reset parameters, we should reset TVP */
	CHKFreeStmt(SQL_RESET_PARAMS, "S");

	count = GET_DESC_FIELD(APP, 0, SQL_DESC_COUNT, SQLSMALLINT);
	CHECK_COND((count == 0, "count %d == 0", (int) count));

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");

	assert(!failed);

	odbc_reset_statement();
}

#ifdef MEMORY_TESTS
static size_t
memory_usage(void)
{
	size_t ret = 0;

	/* mallinfo does not work on Valgrind, ignore */
	if (RUNNING_ON_VALGRIND > 0)
		return ret;

#if defined(HAVE__HEAPWALK)
	{
		_HEAPINFO hinfo;
		int heapstatus;

		hinfo._pentry = NULL;
		while ((heapstatus = _heapwalk(&hinfo)) == _HEAPOK) {
			if (hinfo._useflag == _USEDENTRY)
				ret += hinfo._size;
		}
		assert(heapstatus == _HEAPEMPTY || heapstatus == _HEAPEND);
	}
#elif defined(HAVE_MALLINFO2)
	ret = mallinfo2().uordblks;
#else
	ret = (size_t) (mallinfo().uordblks);
#endif
	return ret;
}

static void
TestInitializeLeak(void)
{
	SQLWCHAR *tableName;
	SQLLEN numRows;
	size_t initial_memory;
	int i;

	tableName = odbc_get_sqlwchar(&odbc_buf, "TVPType");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");

	CHKSetStmtAttr(SQL_SOPT_SS_PARAM_FOCUS, (SQLPOINTER) 1, SQL_IS_INTEGER, "S");

	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, intCol, sizeof(SQLINTEGER), lIntCol, "S");

	/* try to repeat binding column */
	initial_memory = memory_usage();
	for (i = 0; i < 1024; ++i)
		CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, intCol, sizeof(SQLINTEGER), lIntCol, "S");

	/* memory should not increase a lot */
	assert(memory_usage() < initial_memory + 10240);

	odbc_reset_statement();

	/* check we don't leak binding table multiple times */
	/* this leak memory on MS driver */
	if (odbc_driver_is_freetds()) {
		CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");

		/* try to repeat set of the table */
		initial_memory = memory_usage();
		for (i = 0; i < 1024; ++i)
			CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_DEFAULT, SQL_SS_TABLE, MAX_ROWS, 0, tableName, SQL_NTS, &numRows, "S");

		/* memory should not increase a lot */
		assert(memory_usage() < initial_memory + 10240);

		odbc_reset_statement();
	}
}
#endif

TEST_MAIN()
{
	odbc_use_version3 = true;

	setup();

	odbc_connect();

	if (odbc_tds_version() < 0x703) {
		odbc_disconnect();
		printf("TVP data is supported since protocol 7.3, MSSQL only.\n");
		odbc_test_skipped();
		return 0;
	}

	TestTVPInsert();
	TestTVPInsert2();
	TestTVPMemoryManagement();
	TestErrors();
	TestDescriptorValues();

#ifdef MEMORY_TESTS
	TestInitializeLeak();
#endif

	odbc_disconnect();

	return 0;
}
