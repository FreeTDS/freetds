/*
 * Purpose: Test internal query_has_for_update function
 */

#include "common.h"
#include <ctlib.h>

#if ENABLE_EXTRA_CHECKS

static CS_CONTEXT *ctx;
static bool (*query_has_for_update)(const char *query);

static void
test(const char *query, bool expected, int line)
{
	bool res = query_has_for_update(query);
	if (res == expected)
		return;

	fprintf(stderr, "%d:Wrong result: %s instead of %s\nQuery: %s\n",
		line,
		res ? "true": "false",
		expected ? "true": "false",
		query);
	exit(1);
}

#define test(query, expected) test(query, expected, __LINE__)

TEST_MAIN()
{
	check_call(cs_ctx_alloc, (CS_VERSION_100, &ctx));

	check_call(ct_callback, (ctx, NULL, CS_GET, CS_QUERY_HAS_FOR_UPDATE, &query_has_for_update));

	test("", false);
	test("SELECT * FROM table FOR UPDATE", true);
	test("SELECT * FROM table FOR dummy UPDATE", false);
	test("SELECT * FROM table FOR/* comment */UPDATE", true);
	test("SELECT * FROM table FOR--xxx\nUPDATE", true);
	test("SELECT * FROM table /* FOR UPDATE */", false);

	check_call(ct_exit, (ctx, CS_UNUSED));
	check_call(cs_ctx_drop,(ctx));

	return 0;
}

#else

TEST_MAIN()
{
	return 0;
}

#endif

