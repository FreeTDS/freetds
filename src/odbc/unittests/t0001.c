#include "common.h"

static char software_version[] = "$Id: t0001.c,v 1.16 2008-02-08 09:28:04 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{

	int res;
	int i;

	SQLLEN cnamesize;

	const char *command;
	SQLCHAR output[256];

	Connect();

	Command(Statement, "if object_id('tempdb..#odbctestdata') is not null drop table #odbctestdata");

	command = "create table #odbctestdata ("
		"col1 varchar(30) not null,"
		"col2 int not null,"
		"col3 float not null," "col4 numeric(18,6) not null," "col5 datetime not null," "col6 text not null)";
	CHK(CommandWithResult, (Statement, command));

	command = "insert #odbctestdata values ("
		"'ABCDEFGHIJKLMNOP',"
		"123456," "1234.56," "123456.78," "'Sep 11 2001 10:00AM'," "'just to check returned length...')";
	CHK(CommandWithResult, (Statement, command));

	CHK(CommandWithResult, (Statement, "select * from #odbctestdata"));

	res = SQLFetch(Statement);
	if (res != SQL_SUCCESS && res != SQL_SUCCESS_WITH_INFO) {
		fprintf(stderr, "Unable to fetch row\n");
		CheckReturn();
	}

	for (i = 1; i <= 6; i++) {
		CHK(SQLGetData, (Statement, i, SQL_C_CHAR, output, sizeof(output), &cnamesize));

		printf("output data >%s< len_or_ind = %d\n", output, (int) cnamesize);
		if (cnamesize != strlen((char *) output))
			return 1;
	}

	res = SQLFetch(Statement);
	if (res != SQL_NO_DATA) {
		fprintf(stderr, "Unable to fetch row\n");
		CheckReturn();
	}

	res = SQLCloseCursor(Statement);
	if (!SQL_SUCCEEDED(res)) {
		fprintf(stderr, "Unable to close cursor\n");
		CheckReturn();
	}

	Command(Statement, "drop table #odbctestdata");

	Disconnect();

	printf("Done.\n");
	return 0;
}
