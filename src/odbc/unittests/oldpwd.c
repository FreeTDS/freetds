#include "common.h"

/* change password on server */

static void
my_attrs(void)
{
	SQLSetConnectAttr(odbc_conn, 1226 /*SQL_COPT_SS_OLDPWD */,
			  (SQLPOINTER) common_pwd.PASSWORD, SQL_NTS);
	strcpy(common_pwd.PASSWORD, "testpwd$");
}

TEST_MAIN()
{
	odbc_use_version3 = 1;
	odbc_set_conn_attr = my_attrs;
	odbc_connect();

	odbc_disconnect();
	return 0;
}
