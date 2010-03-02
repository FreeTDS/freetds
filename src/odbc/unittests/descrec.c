/* test SQLGetDescRec */
#include "common.h"

static char software_version[] = "$Id: descrec.c,v 1.1 2010-03-02 15:41:37 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(void)
{
	SQLHDESC Descriptor;
	SQLINTEGER ind;
	SQLCHAR name[128];
	SQLSMALLINT si;

	use_odbc_version3 = 1;
	Connect();

	Command("create table #tmp1 (i int)");

	/* get IRD */
	CHKGetStmtAttr(SQL_ATTR_IMP_ROW_DESC, &Descriptor, sizeof(Descriptor), &ind, "S");

	CHKGetDescRec(-1, name, sizeof(name), &si, NULL, NULL, NULL, NULL, NULL, NULL, "E");
	/* TODO here should be NO_DATA cause we are requesting bookmark */
	/* CHKGetDescRec(0, name, sizeof(name), &si, NULL, NULL, NULL, NULL, NULL, NULL, "No"); */
	CHKGetDescRec(1, name, sizeof(name), &si, NULL /*Type*/, NULL /*SubType*/, NULL /*Length*/, NULL/*Precision*/,
		      NULL /*Scale*/, NULL /*Nullable*/, "No");

	Command("SELECT name FROM sysobjects");

	CHKGetDescRec(1, name, sizeof(name), &si, NULL /*Type*/, NULL /*SubType*/, NULL /*Length*/, NULL/*Precision*/,
		      NULL /*Scale*/, NULL /*Nullable*/, "S");

	Disconnect();
	return 0;
}

