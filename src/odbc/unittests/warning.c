#include "common.h"

/*
 * Test originally written by John K. Hohm
 * (cfr "Warning return as copy of last result row (was: Warning: Null value
 * is eliminated by an aggregate or other SET operation.)" July 15th 2006)
 *
 * Contains also similar test by Jeff Dahl
 * (cfr "Warning: Null value is eliminated by an aggregate or other SET 
 * operation." March 24th 2006
 *
 * This test wrong SQLFetch results with warning inside select
 * Is different from raiserror test cause in raiserror error is not
 * inside recordset
 * Sybase do not return warning but test works the same
 */
static char software_version[] = "$Id: warning.c,v 1.4 2007-05-17 07:18:48 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static const char one_null_with_warning[] = "select max(a) as foo from (select convert(int, null) as a) as test";

#ifdef TDS_NO_DM
static const int tds_no_dm = 1;
#else
static const int tds_no_dm = 0;
#endif

static void
Test(const char *query)
{
	int res;

	if (SQLPrepare(Statement, (SQLCHAR *) query, SQL_NTS) != SQL_SUCCESS) {
		fprintf(stderr, "Unable to prepare statement\n");
		exit(1);
	}

	if (SQLExecute(Statement) != SQL_SUCCESS) {
		fprintf(stderr, "Unable to execute statement\n");
		exit(1);
	}

	res = SQLFetch(Statement);
	if (res != SQL_SUCCESS && res != SQL_SUCCESS_WITH_INFO) {
		fprintf(stderr, "Unable to fetch row.\n");
		CheckReturn();
		exit(1);
	}

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		fprintf(stderr, "Warning was returned as a result row -- bad!\n");
		exit(1);
	}

	/*
	 * Microsoft SQL Server 2000 provides a diagnostic record
	 * associated with the second SQLFetch (which returns
	 * SQL_NO_DATA) saying "Warning: Null value is eliminated by
	 * an aggregate or other SET operation."
	 * We check for "NO DM" cause unixODBC till 2.2.11 do not read
	 * errors on SQL_NO_DATA
	 */
	if (db_is_microsoft() && tds_no_dm) {
		SQLCHAR output[256];

		if (!SQL_SUCCEEDED(SQLGetDiagRec(SQL_HANDLE_STMT, Statement, 1, NULL, NULL, output, sizeof(output), NULL))) {
			fprintf(stderr, "SQLGetDiagRec should not fail\n");
			exit(1);
		}
		printf("Message: %s\n", (char *) output);
	}

	ResetStatement();
}

int
main(void)
{
	Connect();

	Command(Statement, "CREATE TABLE #warning(name varchar(20), value int)");
	Command(Statement, "INSERT INTO #warning VALUES('a', NULL)");

	Test(one_null_with_warning);
	Test("SELECT SUM(value) FROM #warning");

	Disconnect();

	printf("Done.\n");
	return 0;
}

