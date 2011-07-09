#include "common.h"
#include <assert.h>

/* Test using array binding */

static char software_version[] = "$Id: array.c,v 1.17 2011-07-09 20:41:10 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static const char *test_query = NULL;

#define XMALLOC_N(t, n) (t*) ODBC_GET(n*sizeof(t))

static void
query_test(int prepare, SQLRETURN expected, const char *expected_status)
{
#define DESC_LEN 51
#define ARRAY_SIZE 10

	ODBC_BUF *odbc_buf = NULL;
	SQLUINTEGER *ids = XMALLOC_N(SQLUINTEGER,ARRAY_SIZE);
	typedef SQLCHAR desc_t[DESC_LEN];
	desc_t *descs = XMALLOC_N(desc_t, ARRAY_SIZE);
	SQLLEN *id_lens = XMALLOC_N(SQLLEN,ARRAY_SIZE), *desc_lens = XMALLOC_N(SQLLEN,ARRAY_SIZE);
	SQLUSMALLINT i, *statuses = XMALLOC_N(SQLUSMALLINT,ARRAY_SIZE);
	SQLULEN processed;
	RETCODE ret;
	char status[20];
	int failure = 0;

	assert(odbc_stmt != SQL_NULL_HSTMT);
	odbc_reset_statement();

	odbc_command_with_result(odbc_stmt, "drop table #tmp1");
	odbc_command("create table #tmp1 (id tinyint, value char(20))");

	SQLSetStmtAttr(odbc_stmt, SQL_ATTR_PARAM_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN, 0);
	SQLSetStmtAttr(odbc_stmt, SQL_ATTR_PARAMSET_SIZE, (void *) ARRAY_SIZE, 0);
	SQLSetStmtAttr(odbc_stmt, SQL_ATTR_PARAM_STATUS_PTR, statuses, 0);
	SQLSetStmtAttr(odbc_stmt, SQL_ATTR_PARAMS_PROCESSED_PTR, &processed, 0);
	SQLBindParameter(odbc_stmt, 1, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 5, 0, ids, 0, id_lens);
	SQLBindParameter(odbc_stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, DESC_LEN - 1, 0, descs, DESC_LEN, desc_lens);

	processed = ARRAY_SIZE + 1;
	for (i = 0; i < ARRAY_SIZE; i++) {
		statuses[i] = SQL_PARAM_DIAG_UNAVAILABLE;
		ids[i] = i * 132;
		sprintf((char *) descs[i], "data %d", i * 7);
		id_lens[i] = 0;
		desc_lens[i] = SQL_NTS;
	}

	if (!prepare) {
		ret = SQLExecDirect(odbc_stmt, (SQLCHAR *) test_query, SQL_NTS);
	} else {
		SQLPrepare(odbc_stmt, (SQLCHAR *) test_query, SQL_NTS);
		ret = SQLExecute(odbc_stmt);
	}
	if (ret != expected) {
		char buf[256];

		sprintf(buf, "Invalid result: got %d exptected %d processed %d", ret, expected, (int) processed);
		ODBC_REPORT_ERROR(buf);
	}

	for (i = 0; i < ARRAY_SIZE; i++)
		SQLMoreResults(odbc_stmt);

	if (processed > ARRAY_SIZE) {
		char buf[256];

		sprintf(buf, "Invalid processed number: %d", (int) processed);
		ODBC_REPORT_ERROR(buf);
	}

	for (i = 0; i < processed; ++i) {
		switch (statuses[i]) {
		case SQL_PARAM_SUCCESS:
		case SQL_PARAM_SUCCESS_WITH_INFO:
			status[i] = 'V';
			break;

		case SQL_PARAM_ERROR:
			status[i] = '!';
			break;

		case SQL_PARAM_UNUSED:
			status[i] = ' ';
			break;

		case SQL_PARAM_DIAG_UNAVAILABLE:
			status[i] = '?';
			break;
		default:
			fprintf(stderr, "Invalid status returned\n");
			exit(1);
		}
	}
	status[i] = 0;

	if (expected_status && strcmp(expected_status, status) != 0) {
		fprintf(stderr, "Invalid status\n\tgot      '%s'\n\texpected '%s'\n", status, expected_status);
		failure = 1;
	}

	odbc_reset_statement();

	ODBC_FREE();
	if (failure) {
		odbc_disconnect();
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	odbc_use_version3 = 1;
	odbc_connect();

	if (odbc_db_is_microsoft()) {
		test_query = "INSERT INTO #tmp1 (id, value) VALUES (?, ?)";
		query_test(0, SQL_ERROR, "VV!!!!!!!!");
		/* FIXME test why is different and what should be correct result */
		query_test(1, odbc_driver_is_freetds() ? SQL_ERROR : SQL_SUCCESS_WITH_INFO, "VV!!!!!!!!");

		test_query = "INSERT INTO #tmp1 (id) VALUES (?) UPDATE #tmp1 SET value = ?";
		query_test(0, SQL_SUCCESS_WITH_INFO, "VVVV!V!V!V");
		/* FIXME test why is different and what should be correct result */
		query_test(1, odbc_driver_is_freetds() ? SQL_ERROR : SQL_SUCCESS_WITH_INFO, "VV!!!!!!!!");

		/* with result, see how SQLMoreResult work */
		test_query = "INSERT INTO #tmp1 (id) VALUES (?) SELECT * FROM #tmp1 UPDATE #tmp1 SET value = ?";
		/* IMHO our driver is better here -- freddy77 */
		query_test(0, SQL_SUCCESS, odbc_driver_is_freetds() ? "VVVVV!V!V!" : "VVVVVV!VVV");
#ifdef ENABLE_DEVELOPING
		query_test(1, SQL_SUCCESS, "VVVVVVVVVV");
#endif
	} else {
		/* Sybase test for conversions before executing */
		test_query = "INSERT INTO #tmp1 (id, value) VALUES (?/8, ?)";
		query_test(0, SQL_SUCCESS, "VVVVVVVVVV");
	}

	/* TODO record binding, array fetch, sqlputdata */

	odbc_disconnect();

	printf("Success!.\n");
	return 0;
}

