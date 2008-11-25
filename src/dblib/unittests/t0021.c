/* 
 * Purpose: Test dbsafestr()
 * Functions: dbsafestr 
 */

#include "common.h"

static char software_version[] = "$Id: t0021.c,v 1.13 2008-11-25 22:58:29 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };



int failed = 0;

/* unsafestr must contain one quote of each type */
const char *unsafestr = "This is a string with ' and \" in it.";

/* safestr must be at least strlen(unsafestr) + 3 */
char safestr[100];

int
main(int argc, char **argv)
{
	int len;
	RETCODE ret;

	set_malloc_options();

	fprintf(stdout, "Starting %s\n", argv[0]);

	/* Fortify_EnterScope(); */
	dbinit();


	len = strlen(unsafestr);
	ret = dbsafestr(NULL, unsafestr, -1, safestr, len, DBSINGLE);
	if (ret != FAIL)
		failed++;
	fprintf(stdout, "short buffer, single\n%s\n", safestr);
	/* plus one for termination and one for the quote */
	ret = dbsafestr(NULL, unsafestr, -1, safestr, len + 2, DBSINGLE);
	if (ret != SUCCEED)
		failed++;
	if (strlen(safestr) != len + 1)
		failed++;
	fprintf(stdout, "single quote\n%s\n", safestr);
	ret = dbsafestr(NULL, unsafestr, -1, safestr, len + 2, DBDOUBLE);
	if (ret != SUCCEED)
		failed++;
	if (strlen(safestr) != len + 1)
		failed++;
	fprintf(stdout, "double quote\n%s\n", safestr);
	ret = dbsafestr(NULL, unsafestr, -1, safestr, len + 3, DBBOTH);
	if (ret != SUCCEED)
		failed++;
	if (strlen(safestr) != len + 2)
		failed++;
	fprintf(stdout, "both quotes\n%s\n", safestr);

	dbexit();

	fprintf(stdout, "dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	return failed ? 1 : 0;
}
