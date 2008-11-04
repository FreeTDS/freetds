#include "common.h"
#include <assert.h>

static char software_version[] = "$Id: getdata.c,v 1.9 2008-11-04 10:59:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static char odbc_err[256];
static char odbc_sqlstate[6];

static void
ReadError(void)
{
	memset(odbc_err, 0, sizeof(odbc_err));
	memset(odbc_sqlstate, 0, sizeof(odbc_sqlstate));
	CHKGetDiagRec(SQL_HANDLE_STMT, Statement, 1, (SQLCHAR *) odbc_sqlstate,
		      NULL, (SQLCHAR *) odbc_err, sizeof(odbc_err), NULL, "SI");
	printf("Message: '%s' %s\n", odbc_sqlstate, odbc_err);
}

static void
test_err(const char *data, int c_type, const char *state)
{
	char sql[128];
	SQLLEN ind;
	const unsigned int buf_size = 128;
	char *buf = (char *) malloc(buf_size);

	sprintf(sql, "SELECT '%s'", data);
	Command(Statement, sql);
	SQLFetch(Statement);
	CHKGetData(1, c_type, buf, buf_size, &ind, "E");
	free(buf);
	ReadError();
	if (strcmp(odbc_sqlstate, state) != 0) {
		fprintf(stderr, "Unexpected sql state returned\n");
		Disconnect();
		exit(1);
	}
	ResetStatement();
}

static int lc;
static int type;

static int
mycmp(const char *s1, const char *s2)
{
	SQLWCHAR buf[128], *wp;
	unsigned l;

	if (type == SQL_C_CHAR)
		return strcmp(s1, s2);

	l = strlen(s2);
	assert(l < (sizeof(buf)/sizeof(buf[0])));
	wp = buf;
	do {
		*wp++ = *s2;
	} while (*s2++);

	return memcmp(s1, buf, l * lc + lc);
}

int
main(int argc, char *argv[])
{
	char buf[16];
	SQLINTEGER int_buf;
	SQLLEN len;

	Connect();

	lc = 1;
	type = SQL_C_CHAR;

	for (;;) {
		/* TODO test with VARCHAR too */
		Command(Statement, "SELECT CONVERT(TEXT,'Prova')");

		CHKFetch("S");

		/* these 2 tests test an old severe BUG in FreeTDS */
		CHKGetData(1, type, buf, 0, NULL, "I");
		CHKGetData(1, type, buf, 0, NULL, "I");
		CHKGetData(1, type, buf, 3 * lc, NULL, "I");
		if (mycmp(buf, "Pr") != 0) {
			printf("Wrong data result 1\n");
			exit(1);
		}

		CHKGetData(1, type, buf, 16, NULL, "S");
		if (mycmp(buf, "ova") != 0) {
			printf("Wrong data result 2 res = '%s'\n", buf);
			exit(1);
		}

		ResetStatement();

		/* test with varchar, not blob but variable */
		Command(Statement, "SELECT CONVERT(VARCHAR(100), 'Other test')");

		CHKFetch("S");

		CHKGetData(1, type, buf, 7 * lc, NULL, "I");
		if (mycmp(buf, "Other ") != 0) {
			printf("Wrong data result 1\n");
			exit(1);
		}

		CHKGetData(1, type, buf, 16, NULL, "S");
		if (mycmp(buf, "test") != 0) {
			printf("Wrong data result 2 res = '%s'\n", buf);
			exit(1);
		}

		ResetStatement();

		if (type != SQL_C_CHAR)
			break;

		type = SQL_C_WCHAR;
		lc = sizeof(SQLWCHAR);
	}

	/* test with fixed length */
	Command(Statement, "SELECT CONVERT(INT, 12345)");

	CHKFetch("S");

	int_buf = 0xdeadbeef;
	CHKGetData(1, SQL_C_SLONG, &int_buf, 0, NULL, "S");
	if (int_buf != 12345) {
		printf("Wrong data result\n");
		exit(1);
	}

	CHKGetData(1, SQL_C_SLONG, &int_buf, 0, NULL, "No");
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

	/* test for empty string from mssql */
	if (db_is_microsoft()) {
		lc = 1;
		type = SQL_C_CHAR;

		for (;;) {
			Command(Statement, "SELECT CONVERT(TEXT,'')");

			CHKFetch("S");

			len = 1234;
			CHKGetData(1, type, buf, lc, &len, "S");

			if (len != 0) {
				fprintf(stderr, "Wrong len returned, returned %ld\n", (long) len);
				return 1;
			}

			CHKGetData(1, type, buf, lc, NULL, "No");
			ResetStatement();

			if (type != SQL_C_CHAR)
				break;
			lc = sizeof(SQLWCHAR);
			type = SQL_C_WCHAR;
		}	
	}

	Disconnect();

	printf("Done.\n");
	return 0;
}
