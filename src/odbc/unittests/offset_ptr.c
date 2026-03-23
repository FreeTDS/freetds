#include "common.h"

/*
 * Row layout for row-wise binding: one value and one length/indicator
 * per column, matching the buffer layout pyodbc uses in ExecuteMulti().
 */
typedef struct
{
	SQLINTEGER id1;
	SQLLEN ind1;
	char ch1[8];
	SQLLEN ind2;
} Row;

/*
 * Minimal reproducible example for a segfault in FreeTDS when
 * SQL_ATTR_PARAM_BIND_OFFSET_PTR was set on a statement handle.
 *
 * Test from Bob Kline
 */
static void
test_insert(void)
{
	const char *sql;
	char *base;

	/* Three rows of actual parameter data. */
	static const Row rows[3] = {
		{10, 0, "hixxx", 2},
		{20, 0, "hello", 5},
		{30, 0, "bye", SQL_NULL_DATA},
	};

	/*
	 * bop: the bind offset.
	 * base(0x10) + bop == &rows[0], so a conformant driver will read
	 * id1=10, ch1='hi' for the first row, id1=20, ch1='hello' for the second
	 * and id1=30, ch1=NULL for the third.
	 */
	SQLULEN bop = (SQLULEN) (TDS_INTPTR) rows - 16;

	CHKPrepare(T("INSERT INTO #offset_ptr(id1, ch1) VALUES (?, ?)"), SQL_NTS, "S");

	/*
	 * Bind parameters using a non-null but deliberately invalid base
	 * pointer (decimal 16 = 0x10). The real data buffer will be supplied
	 * via SQL_ATTR_PARAM_BIND_OFFSET_PTR below. The ODBC spec explicitly
	 * permits this: "either or both the offset and the address to which
	 * the offset is added can be invalid, as long as their sum is a valid
	 * address."
	 */
	base = (char *) (TDS_INTPTR) 16;
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 10, 0, (SQLPOINTER) base, sizeof(SQLINTEGER),
			 (SQLLEN *) (base + TDS_OFFSET(Row, ind1)), "S");
	CHKBindParameter(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0, (SQLPOINTER) (base + TDS_OFFSET(Row, ch1)),
			 sizeof(rows[0].ch1), (SQLLEN *) (base + TDS_OFFSET(Row, ind2)), "S");

	CHKSetStmtAttr(SQL_ATTR_PARAM_BIND_TYPE, (SQLPOINTER) sizeof(Row), SQL_IS_UINTEGER, "S");
	CHKSetStmtAttr(SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER) 3, SQL_IS_UINTEGER, "S");
	CHKSetStmtAttr(SQL_ATTR_PARAM_BIND_OFFSET_PTR, (SQLPOINTER) & bop, SQL_IS_POINTER, "S");

	/* FreeTDS crashed here with SIGSEGV in tds_convert() */
	printf("Calling SQLExecute...\n");
	CHKExecute("SI");

	odbc_reset_statement();

	sql = "IF NOT EXISTS(SELECT * FROM #offset_ptr WHERE id1 = 10 AND ch1 = 'hi') "
		"OR NOT EXISTS(SELECT * FROM #offset_ptr WHERE id1 = 20 AND ch1 = 'hello') "
		"OR NOT EXISTS(SELECT * FROM #offset_ptr WHERE id1 = 30 AND ch1 IS NULL) SELECT 1";
	odbc_check_no_row(sql);

	printf("PASS: three rows inserted.\n");
}

static void
check_row(const Row *row, int idx, int id, const char *s)
{
	bool s_ok;

	if (s)
		s_ok = (strcmp(row->ch1, s) == 0) && (row->ind2 == strlen(s));
	else
		s_ok = (row->ind2 == SQL_NULL_DATA);

	if (row->id1 != id || !s_ok) {
		char ch1[sizeof(row->ch1) + 1];

		memcpy(ch1, row->ch1, sizeof(row->ch1));
		ch1[sizeof(row->ch1)] = 0;
		fprintf(stderr, "Wrong row %d: id %d ch1 %s ind2 %d\n", idx, (int) row->id1, ch1, (int) row->ind2);
		exit(1);
	}
}

typedef enum
{
	FETCH_NORMAL,
	FETCH_SCROLL,
	FETCH_EXTENDED,
} FetchType;

static void
test_fetch(FetchType fetch_type)
{
	char *base;
	Row rows[3];
	SQLULEN bop = (SQLULEN) (TDS_INTPTR) rows - 16;

#ifdef HAVE_SQLROWSETSIZE
	SQLROWSETSIZE set_size = 0;
#else
	SQLULEN set_size = 0;
#endif

	memset(rows, '*', sizeof(rows));

	CHKPrepare(T("SELECT * FROM #offset_ptr ORDER BY id1"), SQL_NTS, "S");

	base = (char *) (TDS_INTPTR) 16;
	CHKBindCol(1, SQL_C_SLONG, (SQLPOINTER) base, sizeof(SQLINTEGER), (SQLLEN *) (base + TDS_OFFSET(Row, ind1)), "S");
	CHKBindCol(2, SQL_C_CHAR, (SQLPOINTER) (base + TDS_OFFSET(Row, ch1)), sizeof(rows[0].ch1),
		   (SQLLEN *) (base + TDS_OFFSET(Row, ind2)), "S");

	CHKSetStmtAttr(SQL_ATTR_ROW_BIND_TYPE, (SQLPOINTER) sizeof(Row), SQL_IS_UINTEGER, "S");
	if (fetch_type != FETCH_EXTENDED)
		CHKSetStmtAttr(SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) 3, SQL_IS_UINTEGER, "S");
	else
		CHKSetStmtAttr(SQL_ROWSET_SIZE, (SQLPOINTER) TDS_INT2PTR(3), 0, "S");
	CHKSetStmtAttr(SQL_ATTR_ROW_BIND_OFFSET_PTR, (SQLPOINTER) & bop, SQL_IS_POINTER, "S");

	printf("Calling SQLExecute...\n");
	CHKExecute("SI");

	switch (fetch_type) {
	case FETCH_NORMAL:
		CHKFetch("S");
		break;
	case FETCH_SCROLL:
		CHKFetchScroll(SQL_FETCH_NEXT, 0, "S");
		break;
	case FETCH_EXTENDED:
		CHKExtendedFetch(SQL_FETCH_NEXT, 0, &set_size, NULL, "S");
		break;
	}
	while (CHKMoreResults("SNo") == SQL_SUCCESS)
		continue;

	odbc_reset_statement();

	check_row(&rows[0], 0, 10, "hi");
	check_row(&rows[1], 1, 20, "hello");
	check_row(&rows[2], 2, 30, NULL);

	printf("PASS: three rows returned.\n");
}

TEST_MAIN()
{
	odbc_use_version3 = true;
	odbc_connect();

	odbc_command("CREATE TABLE #offset_ptr (id1 INT, ch1 VARCHAR(10) NULL)");

	test_insert();

	test_fetch(FETCH_NORMAL);
	test_fetch(FETCH_SCROLL);
	test_fetch(FETCH_EXTENDED);

	odbc_disconnect();
	return 0;
}
