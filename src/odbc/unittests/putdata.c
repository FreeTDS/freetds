#include "common.h"

/* Test for SQLPutData */

static char software_version[] = "$Id: putdata.c,v 1.7 2004-08-11 12:04:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static const char test_text[] =
	"Nel mezzo del cammin di nostra vita\n" "mi ritrovai per una selva oscura\n" "che' la diritta via era smarrita.";

#define BYTE_AT(n) (((n) * 245 + 123) & 0xff)

int
main(int argc, char *argv[])
{
	SQLINTEGER ind;
	int len = strlen(test_text), n, i;
	const char *p;
	SQLPOINTER ptr;
	unsigned char buf[256], *pb;
	SQLRETURN retcode;

	Connect();

	/* create table to hold data */
	Command(Statement, "CREATE TABLE #putdata (c TEXT NULL, b IMAGE NULL)");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARCHAR, 0, 0, (SQLPOINTER) 123, 0, &ind) !=
	    SQL_SUCCESS) {
		printf("Unable to bind output parameter\n");
		exit(1);
	}
	/* length required */
	ind = SQL_LEN_DATA_AT_EXEC(len);

	/* 
	 * test for char 
	 */

	if (SQLPrepare(Statement, (SQLCHAR *) "INSERT INTO #putdata(c) VALUES(?)", SQL_NTS) != SQL_SUCCESS) {
		printf("Unable to prepare statement\n");
		exit(1);
	}

	if (SQLExecute(Statement) != SQL_NEED_DATA) {
		printf("Wrong result executing statement\n");
		exit(1);
	}

	p = test_text;
	n = 5;
	if (SQLParamData(Statement, &ptr) != SQL_NEED_DATA) {
		printf("Wrong result from SQLParamData\n");
		exit(1);
	}
	if (ptr != (SQLPOINTER) 123) {
		printf("Wrong pointer from SQLParamData\n");
		exit(1);
	}
	while (*p) {
		int l = strlen(p);

		if (l < n)
			n = l;
		if (SQLPutData(Statement, (char *) p, n) != SQL_SUCCESS) {
			printf("Wrong result from SQLPutData\n");
			exit(1);
		}
		p += n;
		n *= 2;
	}
	if (SQLParamData(Statement, &ptr) != SQL_SUCCESS) {
		printf("Wrong result from SQLParamData\n");
		exit(1);
	}

	if (SQLParamData(Statement, &ptr) != SQL_ERROR) {
		printf("Wrong result from SQLParamData\n");
		exit(1);
	}

	/* check state  and reset some possible buffers */
	Command(Statement, "DECLARE @i INT");

	/* update row setting binary field */
	for (i = 0; i < 255; ++i)
		buf[i] = BYTE_AT(i);

	/* 
	 * test for binary 
	 */

	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARBINARY, 0, 0, (SQLPOINTER) 4567, 0, &ind) !=
	    SQL_SUCCESS) {
		printf("Unable to bind output parameter\n");
		exit(1);
	}
	ind = SQL_LEN_DATA_AT_EXEC(254);

	if (SQLPrepare(Statement, (SQLCHAR *) "UPDATE #putdata SET b = ?", SQL_NTS) != SQL_SUCCESS) {
		printf("Unable to prepare statement\n");
		exit(1);
	}

	if (SQLExecute(Statement) != SQL_NEED_DATA) {
		printf("Wrong result executing statement\n");
		exit(1);
	}

	pb = buf;
	n = 7;
	if (SQLParamData(Statement, &ptr) != SQL_NEED_DATA) {
		printf("Wrong result from SQLParamData\n");
		exit(1);
	}
	if (ptr != (SQLPOINTER) 4567) {
		printf("Wrong pointer from SQLParamData\n");
		exit(1);
	}
	while (pb != (buf + 254)) {
		int l = buf + 254 - pb;

		if (l < n)
			n = l;
		if (SQLPutData(Statement, (char *) p, n) != SQL_SUCCESS) {
			printf("Wrong result from SQLPutData\n");
			exit(1);
		}
		pb += n;
		n *= 2;
	}
	if (SQLParamData(Statement, &ptr) != SQL_SUCCESS) {
		printf("Wrong result from SQLParamData\n");
		exit(1);
	}

	if (SQLParamData(Statement, &ptr) != SQL_ERROR) {
		printf("Wrong result from SQLParamData\n");
		exit(1);
	}

	/* check state  and reset some possible buffers */
	Command(Statement, "DECLARE @i2 INT");


	/* test len == 0 case from ML */
	if (SQLFreeStmt(Statement, SQL_RESET_PARAMS) != SQL_SUCCESS) {
		printf("SQLFreeStmt error\n");
		exit(1);
	}

	if (SQLPrepare(Statement, (SQLCHAR *) "INSERT INTO #putdata(c) VALUES(?)", SQL_NTS) != SQL_SUCCESS) {
		printf("Unable to prepare statement\n");
		exit(1);
	}

	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARCHAR, 0, 0, (PTR) 2, 0, &ind) != SQL_SUCCESS) {
		printf("SQLBindParameter error\n");
		exit(1);
	}

	ind = SQL_LEN_DATA_AT_EXEC(0);

	if ((retcode = SQLExecute(Statement)) != SQL_NEED_DATA) {
		printf("Wrong result executing statement (retcode=%d)\n", (int) retcode);
		exit(1);
	}
	while (retcode == SQL_NEED_DATA) {
		retcode = SQLParamData(Statement, &ptr);
		if (retcode == SQL_NEED_DATA) {
			SQLPutData(Statement, "abc", 3);
		}
	}

	/* TODO check inserts ... */
	/* TODO test cancel inside SQLExecute */

	Disconnect();

	printf("Done.\n");
	return 0;
}
