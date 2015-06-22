#include "common.h"

/* change password on server */

static void
my_attrs(void)
{
	SQLSetConnectAttr(odbc_conn, 1226 /*SQL_COPT_SS_OLDPWD */, (SQLPOINTER) odbc_password, SQL_NTS);
	strcpy(odbc_password, "testpwd$");
}

int
main(int argc, char *argv[])
{
	odbc_use_version3 = 1;
	odbc_set_conn_attr = my_attrs;
	odbc_connect();

	odbc_disconnect();
	return 0;
}
