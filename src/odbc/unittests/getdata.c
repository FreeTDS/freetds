#include "common.h"

static char software_version[] = "$Id: getdata.c,v 1.4.2.3 2009-12-07 16:07:31 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static char odbc_err[256];
static char odbc_sqlstate[6];

static void
ReadError(void)
{
	memset(odbc_err, 0, sizeof(odbc_err));
	memset(odbc_sqlstate, 0, sizeof(odbc_sqlstate));
	if (!SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT
					, Statement
					, 1
					, (SQLCHAR *) odbc_sqlstate
					, NULL
					, (SQLCHAR *) odbc_err
					, sizeof(odbc_err)
					, NULL))) {
		printf("SQLGetDiagRec should not fail\n");
		exit(1);
	}
	printf("Message: '%s' %s\n", odbc_sqlstate, odbc_err);
}

static void
test_err(const char *data, int c_type, const char *state)
{
	char sql[128];
	SQLRETURN rc;
	SQLLEN ind;
	const unsigned int buf_size = 128;
	char *buf = (char *) malloc(buf_size);

	sprintf(sql, "SELECT '%s'", data);
	Command(Statement, sql);
	SQLFetch(Statement);
	rc = SQLGetData(Statement, 1, c_type, buf, buf_size, &ind);
	free(buf);
	if (rc != SQL_ERROR)
		ODBC_REPORT_ERROR("SQLGetData error expected");
	ReadError();
	if (strcmp(odbc_sqlstate, state) != 0) {
		fprintf(stderr, "Unexpected sql state returned\n");
		Disconnect();
		exit(1);
	}
	ResetStatement();
}

int
main(int argc, char *argv[])
{
	char buf[16];
	SQLINTEGER int_buf;
	SQLLEN len;

	Connect();

	/* TODO test with VARCHAR too */
	Command(Statement, "SELECT CONVERT(TEXT,'Prova')");

	if (SQLFetch(Statement) != SQL_SUCCESS) {
		printf("Unable to fetch row\n");
		CheckReturn();
		exit(1);
	}

	/* these 2 tests test an old severe BUG in FreeTDS */
	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 0, NULL) != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("Unable to get data");

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 0, NULL) != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("Unable to get data");

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 3, NULL) != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("Unable to get data");
	if (strcmp(buf, "Pr") != 0) {
		printf("Wrong data result 1\n");
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 16, NULL) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to get data");
	if (strcmp(buf, "ova") != 0) {
		printf("Wrong data result 2 res = '%s'\n", buf);
		exit(1);
	}

	ResetStatement();

	/* test with varchar, not blob but variable */
	Command(Statement, "SELECT CONVERT(VARCHAR(100), 'Other test')");

	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to fetch row");

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 7, NULL) != SQL_SUCCESS_WITH_INFO)
		ODBC_REPORT_ERROR("Unable to get data");
	if (strcmp(buf, "Other ") != 0) {
		printf("Wrong data result 1\n");
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 16, NULL) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to get data");
	if (strcmp(buf, "test") != 0) {
		printf("Wrong data result 2 res = '%s'\n", buf);
		exit(1);
	}

	ResetStatement();

	/* test with fixed length */
	Command(Statement, "SELECT CONVERT(INT, 12345)");

	if (SQLFetch(Statement) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to fetch row");

	int_buf = 0xdeadbeef;
	if (SQLGetData(Statement, 1, SQL_C_SLONG, &int_buf, 0, NULL) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to get data");
	if (int_buf != 12345) {
		printf("Wrong data result\n");
		exit(1);
	}

	if (SQLGetData(Statement, 1, SQL_C_SLONG, &int_buf, 0, NULL) != SQL_NO_DATA)
		ODBC_REPORT_ERROR("Unable to get data");
	if (int_buf != 12345) {
		printf("Wrong data result 2 res = %d\n", (int) int_buf);
		exit(1);
	}

	ResetStatement();

	Disconnect();

	use_odbc_version3 = 1;
	Connect();

	/* test error from SQLGetData */
	/* wrong constant */
	test_err("prova 123",           SQL_VARCHAR,     "HY003");
	/* use ARD but no ARD data column */
	test_err("prova 123",           SQL_ARD_TYPE,    "07009");
	/* wrong conversion, int */
	test_err("prova 123",           SQL_C_LONG,      "22018");
	/* wrong conversion, int */
	test_err("prova 123",           SQL_C_TIMESTAMP, "22018");
	/* overflow */
	test_err("1234567890123456789", SQL_C_LONG,      "22003");

	if (db_is_microsoft()) {
		Command(Statement, "SELECT CONVERT(TEXT,'')");

		if (SQLFetch(Statement) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("Unable to fetch row");

		len = 1234;
		if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 1, &len) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("invalid return from SQLGetData");

		if (len != 0) {
			fprintf(stderr, "Wrong len returned, returned %ld\n", (long) len);
			return 1;
		}

		if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 1, NULL) != SQL_NO_DATA)
			ODBC_REPORT_ERROR("invalid return from SQLGetData");

		ResetStatement();

		Command(Statement, "SELECT CONVERT(TEXT,'')");

		if (SQLFetch(Statement) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("Unable to fetch row");

		len = 1234;
		if (SQLGetData(Statement, 1, SQL_C_BINARY, buf, 1, &len) != SQL_SUCCESS)
			ODBC_REPORT_ERROR("invalid return from SQLGetData");

		if (len != 0) {
			fprintf(stderr, "Wrong len returned, returned %ld\n", (long) len);
			return 1;
		}

		if (SQLGetData(Statement, 1, SQL_C_CHAR, buf, 1, NULL) != SQL_NO_DATA)
			ODBC_REPORT_ERROR("invalid return from SQLGetData");
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
