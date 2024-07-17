/*
 * Purpose: Test dbsafestr()
 * Functions: dbsafestr
 */

#include "common.h"

#ifndef DBNTWIN32

static int failed = 0;

/* unsafestr must contain one quote of each type */
static const char unsafestr[] = "This is a string with ' and \" in it.";

/* safestr must be at least strlen(unsafestr) + 3 */
static char safestr[100];

TEST_MAIN()
{
	int len;
	RETCODE ret;

	set_malloc_options();

	printf("Starting %s\n", argv[0]);

	dbinit();


	len = sizeof(unsafestr) - 1;
	ret = dbsafestr(NULL, unsafestr, -1, safestr, len, DBSINGLE);
	if (ret != FAIL)
		failed++;
	printf("short buffer, single\n%s\n", safestr);
	/* plus one for termination and one for the quote */
	ret = dbsafestr(NULL, unsafestr, -1, safestr, len + 2, DBSINGLE);
	if (ret != SUCCEED)
		failed++;
	if (strlen(safestr) != len + 1)
		failed++;
	printf("single quote\n%s\n", safestr);
	ret = dbsafestr(NULL, unsafestr, -1, safestr, len + 2, DBDOUBLE);
	if (ret != SUCCEED)
		failed++;
	if (strlen(safestr) != len + 1)
		failed++;
	printf("double quote\n%s\n", safestr);
	ret = dbsafestr(NULL, unsafestr, -1, safestr, len + 2, DBBOTH);
	if (ret != FAIL)
		failed++;
	ret = dbsafestr(NULL, unsafestr, -1, safestr, len + 3, DBBOTH);
	if (ret != SUCCEED)
		failed++;
	if (strlen(safestr) != len + 2)
		failed++;
	printf("both quotes\n%s\n", safestr);

	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
#else
TEST_MAIN()
{
	fprintf(stderr, "Not supported by MS DBLib\n");
	return 0;
}
#endif
