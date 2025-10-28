/* test SQLGetDescRec */
#include "common.h"

TEST_MAIN()
{
	SQLHDESC Descriptor;
	SQLINTEGER ind;
	SQLTCHAR name[128];
	SQLSMALLINT si;

	odbc_use_version3 = true;
	odbc_connect();

	odbc_command("create table #tmp1 (i int)");

	/* get IRD */
	CHKGetStmtAttr(SQL_ATTR_IMP_ROW_DESC, &Descriptor, sizeof(Descriptor), &ind, "S");

	CHKGetDescRec(-1, name, TDS_VECTOR_SIZE(name), &si, NULL, NULL, NULL, NULL, NULL, NULL, "E");
	/* TODO here should be NO_DATA cause we are requesting bookmark */
	/* CHKGetDescRec(0, name, sizeof(name), &si, NULL, NULL, NULL, NULL, NULL, NULL, "No"); */
	CHKGetDescRec(1, name, TDS_VECTOR_SIZE(name), &si, NULL /*Type*/, NULL /*SubType*/, NULL /*Length*/, NULL/*Precision*/,
		      NULL /*Scale*/, NULL /*Nullable*/, "No");

	odbc_command("SELECT name FROM sysobjects");

	CHKGetDescRec(1, name, TDS_VECTOR_SIZE(name), &si, NULL /*Type*/, NULL /*SubType*/, NULL /*Length*/, NULL/*Precision*/,
		      NULL /*Scale*/, NULL /*Nullable*/, "S");

	odbc_disconnect();
	ODBC_FREE();
	return 0;
}

