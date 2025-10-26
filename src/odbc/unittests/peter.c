#include "common.h"
#include <assert.h>

/*
	Test SQLNumResultCols after SQLFreeStmt
	This test on Sybase should not raise an error
*/

TEST_MAIN()
{
	SQLSMALLINT num_params, cols;
	SQLLEN count;
	SQLINTEGER id;

	odbc_use_version3 = true;
	odbc_connect();

	odbc_command("create table #tester (id int not null, name varchar(20) not null)");
	odbc_command("insert into #tester(id, name) values(1, 'abc')");
	odbc_command("insert into #tester(id, name) values(2, 'duck')");

	CHKPrepare(T("SELECT * FROM #tester WHERE id = ?"), SQL_NTS, "S");

	CHKNumParams(&num_params, "S");
	assert(num_params == 1);

	id = 1;
	CHKBindParameter(1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &id, sizeof(id), NULL, "S");

	CHKExecute("S");

	CHKFreeStmt(SQL_RESET_PARAMS, "S");

	CHKRowCount(&count, "S");

	CHKNumResultCols(&cols, "S");
	assert(cols == 2);

	odbc_disconnect();
	return 0;
}

