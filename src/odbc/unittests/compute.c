#include "common.h"
#include <assert.h>

/* Test compute results */

/*
 * This it's quite important cause it test different result types
 * mssql odbc have also some extension not supported by FreeTDS
 * and declared in odbcss.h
 */

static char software_version[] = "$Id: compute.c,v 1.1 2004-12-11 13:42:00 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	Connect();

	Command(Statement, "create table #tmp1 (c varchar(20), i int)");
	Command(Statement, "insert into #tmp1 values('pippo', 12)");
	Command(Statement, "insert into #tmp1 values('pippo', 34)");
	Command(Statement, "insert into #tmp1 values('pluto', 1)");
	Command(Statement, "insert into #tmp1 values('pluto', 2)");
	Command(Statement, "insert into #tmp1 values('pluto', 3)");

	/* select * from #tmp1 compute sum(i) */
	/* select * from #tmp1 order by c compute sum(i) by c */

	/* TODO finish */

	Disconnect();
	return 0;
}
