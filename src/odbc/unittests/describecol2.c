#include "common.h"
#include <ctype.h>

/*
 * SQLDescribeCol test
 */
static int g_result = 0;

static void
do_check(int c, const char *test, int line)
{
	if (c)
		return;

	fprintf(stderr, "Failed check %s at line %d\n", test, line);
	g_result = 1;
}

#define check(s) do_check(s, #s, __LINE__)

TEST_MAIN()
{
	SQLSMALLINT len, type;
	SQLTCHAR name[128];

	odbc_connect();
	odbc_command("create table #dc (col_name int, name2 varchar(100))");

	odbc_command("select * from #dc");

	len = 0x1234;
	CHKDescribeCol(1, NULL, 0, &len, &type, NULL, NULL, NULL, "S");
	check(len == 8);

	len = 0x1234;
	CHKDescribeCol(2, name, 0, &len, &type, NULL, NULL, NULL, "I");
	check(len == 5);

	len = 0x1234;
	CHKDescribeCol(1, NULL, 2, &len, &type, NULL, NULL, NULL, "S");
	check(len == 8);

	len = 0x1234;
	strcpy((char *) name, "xxx");
	CHKDescribeCol(2, name, 3, &len, &type, NULL, NULL, NULL, "I");
	check(len == 5 && strcmp(C(name), "na") == 0);

	len = 0x1234;
	strcpy((char *) name, "xxx");
	CHKDescribeCol(1, name, 1, &len, &type, NULL, NULL, NULL, "I");
	check(len == 8 && strcmp(C(name), "") == 0);

	len = 0x1234;
	strcpy((char *) name, "xxx");
	CHKDescribeCol(2, name, 6, &len, &type, NULL, NULL, NULL, "S");
	check(len == 5 && strcmp(C(name), "name2") == 0);

	odbc_disconnect();

	if (g_result == 0)
		printf("Done.\n");
	return g_result;
}
