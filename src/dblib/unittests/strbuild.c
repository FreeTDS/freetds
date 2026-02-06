/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Purpose: Test dbstrbuild function
 */

#include "common.h"

static bool failed = false;

static void
test(const char *args, const char *output, RETCODE rc, const char *expected, int line)
{
	printf("RETCODE %d\n", rc);

	if (strcmp(output, expected) == 0)
		return;

	failed = true;
	fprintf(stderr, "Wrong formatting\n");
	fprintf(stderr, "  output: \"%s\"\n", output);
	fprintf(stderr, "expected: \"%s\"\n", expected);
	fprintf(stderr, "%d: dbstrbuild%s\n", line, args);
}

#define COMMON_ NULL, buf, sizeof(buf),
#define TEST(args, out) do { \
	memset(buf, 0, sizeof(buf)); \
	rc = dbstrbuild args; \
	test(#args, buf, rc, out, __LINE__); \
} while(0)

TEST_MAIN()
{
	int rc;
	char buf[128];

	TEST((COMMON_ "%1!", "%d", 123), "123");
	TEST((COMMON_ "%1!  %2!", "%d %d", 123, 456), "123  456");
	TEST((COMMON_ "%1!", "%s", "   xxx "), "   xxx ");
	TEST((COMMON_ "%1!", "  %6s", "one"), "   one");

	/* floating point with precision */
	TEST((COMMON_ "%1!", "%.3g", 1.23), "1.23");

	/* hexadecimal with flag */
	TEST((COMMON_ "%1!", "%#x", 2), "0x2");

	/* if wrong placeholder stops */
	TEST((COMMON_ "%1! two %2! other %3!", "%d", 123), "123 two ");

#if defined(DBTDS_7_4)
	/* \377 character should never appear, but make sure we don't overflow */
	TEST((COMMON_ "%1!", "%s", "start \377\377\377\377 end"), "start ");
#endif

	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
