#include "common.h"

/* Test for SQLPutData */

static char software_version[] = "$Id: putdata.c,v 1.4 2003-11-05 17:31:31 jklowden Exp $";
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

	Connect();

	/* create table to hold data, we don't use long data */
	Command(Statement, "CREATE TABLE #putdata (c VARCHAR(255) NULL, b VARBINARY(255) NULL)");

	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, (SQLPOINTER) 123, 0, &ind) !=
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
	while (*p) {
		int l = strlen(p);

		if (l < n)
			n = l;
		if (SQLParamData(Statement, &ptr) != SQL_NEED_DATA) {
			printf("Wrong result from SQLParamData\n");
			exit(1);
		}
		if (ptr != (SQLPOINTER) 123) {
			printf("Wrong pointer from SQLParamData\n");
			exit(1);
		}
		SQLPutData(Statement, (char *) p, n);
		p += n;
		n *= 2;
	}

	if (SQLParamData(Statement, &ptr) == SQL_NEED_DATA) {
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

	if (SQLBindParameter(Statement, 1, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_VARBINARY, 0, 0, (SQLPOINTER) 4567, 0, &ind) !=
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
	while (pb != (buf + 254)) {
		int l = buf + 254 - pb;

		if (l < n)
			n = l;
		if (SQLParamData(Statement, &ptr) != SQL_NEED_DATA) {
			printf("Wrong result from SQLParamData\n");
			exit(1);
		}
		if (ptr != (SQLPOINTER) 4567) {
			printf("Wrong pointer from SQLParamData\n");
			exit(1);
		}
		SQLPutData(Statement, pb, n);
		pb += n;
		n *= 2;
	}

	if (SQLParamData(Statement, &ptr) == SQL_NEED_DATA) {
		printf("Wrong result from SQLParamData\n");
		exit(1);
	}

	/* check state  and reset some possible buffers */
	Command(Statement, "DECLARE @i2 INT");

	/* TODO check inserts ... */
	/* TODO test cancel inside SQLExecute */

	Disconnect();

	printf("Done.\n");
	return 0;
}
