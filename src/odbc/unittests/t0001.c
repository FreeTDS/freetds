#include "common.h"

TEST_MAIN()
{

	int i;

	SQLLEN cnamesize;

	const char *command;
	SQLCHAR output[256];

	odbc_connect();

	odbc_command("if object_id('tempdb..#odbctestdata') is not null drop table #odbctestdata");

	command = "create table #odbctestdata ("
		"col1 varchar(30) not null,"
		"col2 int not null,"
		"col3 float not null," "col4 numeric(18,6) not null," "col5 datetime not null," "col6 text not null)";
	odbc_command(command);

	command = "insert #odbctestdata values ("
		"'ABCDEFGHIJKLMNOP',"
		"123456," "1234.56," "123456.78," "'Sep 11 2001 10:00AM'," "'just to check returned length...')";
	odbc_command(command);

	odbc_command("select * from #odbctestdata");

	CHKFetch("SI");

	for (i = 1; i <= 6; i++) {
		CHKGetData(i, SQL_C_CHAR, output, sizeof(output), &cnamesize, "S");

		printf("output data >%s< len_or_ind = %d\n", output, (int) cnamesize);
		if (cnamesize != strlen((char *) output))
			return 1;
	}

	CHKFetch("No");

	CHKCloseCursor("SI");

	odbc_command("drop table #odbctestdata");

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}
