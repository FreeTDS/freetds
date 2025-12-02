/*
 * Test conversions to date and CS_DT_CONVFMT
 */
#include "common.h"

#include <freetds/replacements.h>

static CS_CONTEXT *ctx;
static bool failed = false;
static const char *test_name = NULL;

typedef struct
{
	CS_INT convfmt;
	const char *convfmt_name;
	const char *datetime;
	const char *date;
	const char *time;
} TEST;

#define ONE(convfmt, datetime, date, time) \
	{ convfmt, #convfmt, datetime, date, time }
static const TEST tests[] = {
	ONE(CS_DATES_SHORT, "Sep  7 2025  4:32PM", "Sep  7 2025", " 4:32PM"),
	ONE(CS_DATES_MDY1, "09/07/25", "09/07/25", ""),
	ONE(CS_DATES_YMD1, "25.09.07", "25.09.07", ""),
	ONE(CS_DATES_DMY1, "07/09/25", "07/09/25", ""),
	ONE(CS_DATES_DMY2, "07.09.25", "07.09.25", ""),
	ONE(CS_DATES_DMY3, "07-09-25", "07-09-25", ""),
	ONE(CS_DATES_DMY4, "07 Sep 25", "07 Sep 25", ""),
	ONE(CS_DATES_MDY2, "Sep 07, 25", "Sep 07, 25", ""),
	ONE(CS_DATES_HMS, "16:32:53", "00:00:00", "16:32:53"),
	ONE(CS_DATES_LONG, "Sep  7 2025  4:32:53:490PM", "Sep  7 2025", " 4:32:53:490PM"),
	ONE(CS_DATES_MDY3, "09-07-25", "09-07-25", ""),
	ONE(CS_DATES_YMD2, "25/09/07", "25/09/07", ""),
	ONE(CS_DATES_YMD3, "250907", "250907", ""),
	ONE(CS_DATES_YDM1, "25/07/09", "25/07/09", ""),
	ONE(CS_DATES_MYD1, "09/25/07", "09/25/07", ""),
	ONE(CS_DATES_DYM1, "07/25/09", "07/25/09", ""),
	ONE(CS_DATES_MDYHMS, "Sep  7 2025 16:32:53", "Sep  7 2025 00:00:00", "Jan  1 1900 16:32:53"),
	ONE(CS_DATES_HMA, " 4:32PM", "12:00AM", " 4:32PM"),
	ONE(CS_DATES_HM, "16:32", "00:00", "16:32"),
	ONE(CS_DATES_HMSZA, " 4:32:53:490PM", "12:00:00:000AM", " 4:32:53:490PM"),
	ONE(CS_DATES_HMSZ, "16:32:53:490", "00:00:00:000", "16:32:53:490"),
	ONE(CS_DATES_YMDHMS, "25/09/07 16:32:53", "25/09/07 00:00:00", "00/01/01 16:32:53"),
	ONE(CS_DATES_YMDHMA, "25/09/07  4:32PM", "25/09/07 12:00AM", "00/01/01  4:32PM"),
	ONE(CS_DATES_YMDTHMS, "2025-09-07T16:32:53", "2025-09-07T00:00:00", "1900-01-01T16:32:53"),
	ONE(CS_DATES_HMSUSA, " 4:32:53.490000PM", "12:00:00.000000AM", " 4:32:53.490000PM"),
	ONE(CS_DATES_HMSUS, "16:32:53.490000", "00:00:00.000000", "16:32:53.490000"),
	ONE(CS_DATES_LONGUSA, "Sep  7 25  4:32:53.490000PM", "Sep  7 25 12:00:00.000000AM", "Jan  1 00  4:32:53.490000PM"),
	ONE(CS_DATES_LONGUS, "Sep  7 25 16:32:53.490000", "Sep  7 25 00:00:00.000000", "Jan  1 00 16:32:53.490000"),
	ONE(CS_DATES_YMDHMSUS, "25-09-07 16:32:53.490000", "25-09-07 00:00:00.000000", "00-01-01 16:32:53.490000"),
	ONE(CS_DATES_SHORT_ALT, "Sep  7 2025  4:32PM", "Sep  7 2025 12:00AM", "Jan  1 1900  4:32PM"),
	ONE(CS_DATES_MDY1_YYYY, "09/07/2025", "09/07/2025", ""),
	ONE(CS_DATES_YMD1_YYYY, "2025.09.07", "2025.09.07", ""),
	ONE(CS_DATES_DMY1_YYYY, "07/09/2025", "07/09/2025", ""),
	ONE(CS_DATES_DMY2_YYYY, "07.09.2025", "07.09.2025", ""),
	ONE(CS_DATES_DMY3_YYYY, "07-09-2025", "07-09-2025", ""),
	ONE(CS_DATES_DMY4_YYYY, "07 Sep 2025", "07 Sep 2025", ""),
	ONE(CS_DATES_MDY2_YYYY, "Sep 07, 2025", "Sep 07, 2025", ""),
	ONE(CS_DATES_HMS_ALT, "16:32:53", "00:00:00", "16:32:53"),
	ONE(CS_DATES_LONG_ALT, "Sep  7 2025  4:32:53:490PM", "Sep  7 2025 12:00:00:000AM", "Jan  1 1900  4:32:53:490PM"),
	ONE(CS_DATES_MDY3_YYYY, "09-07-2025", "09-07-2025", ""),
	ONE(CS_DATES_YMD2_YYYY, "2025/09/07", "2025/09/07", ""),
	ONE(CS_DATES_YMD3_YYYY, "20250907", "20250907", ""),
	ONE(CS_DATES_YDM1_YYYY, "2025/07/09", "2025/07/09", ""),
	ONE(CS_DATES_MYD1_YYYY, "09/2025/07", "09/2025/07", ""),
	ONE(CS_DATES_DYM1_YYYY, "07/2025/09", "07/2025/09", ""),
	ONE(CS_DATES_MDYHMS_ALT, "Sep  7 2025 16:32:53", "Sep  7 2025 00:00:00", "Jan  1 1900 16:32:53"),
	ONE(CS_DATES_HMA_ALT, " 4:32PM", "12:00AM", " 4:32PM"),
	ONE(CS_DATES_HM_ALT, "16:32", "00:00", "16:32"),
	ONE(CS_DATES_YMDHMS_YYYY, "2025/09/07 16:32:53", "2025/09/07 00:00:00", "1900/01/01 16:32:53"),
	ONE(CS_DATES_YMDHMA_YYYY, "2025/09/07  4:32PM", "2025/09/07 12:00AM", "1900/01/01  4:32PM"),
	ONE(CS_DATES_HMSUSA_YYYY, " 4:32:53.490000PM", "12:00:00.000000AM", " 4:32:53.490000PM"),
	ONE(CS_DATES_HMSUS_YYYY, "16:32:53.490000", "00:00:00.000000", "16:32:53.490000"),
	ONE(CS_DATES_LONGUSA_YYYY, "Sep  7 2025  4:32:53.490000PM", "Sep  7 2025 12:00:00.000000AM",
	    "Jan  1 1900  4:32:53.490000PM"),
	ONE(CS_DATES_LONGUS_YYYY, "Sep  7 2025 16:32:53.490000", "Sep  7 2025 00:00:00.000000", "Jan  1 1900 16:32:53.490000"),
	ONE(CS_DATES_YMDHMSUS_YYYY, "2025-09-07 16:32:53.490000", "2025-09-07 00:00:00.000000", "1900-01-01 16:32:53.490000"),
	{0, NULL, NULL, NULL, NULL}
};

static void
single_value(CS_INT type, void *input, size_t input_size, const char *expected)
{
	CS_DATAFMT destfmt, srcfmt;
	CS_INT reslen;
	char buffer[1024];

	memset(&destfmt, 0, sizeof(destfmt));
	destfmt.datatype = CS_CHAR_TYPE;
	destfmt.maxlength = sizeof(buffer);
	destfmt.format = CS_FMT_UNUSED;

	memset(&srcfmt, 0, sizeof(srcfmt));
	srcfmt.datatype = type;
	srcfmt.maxlength = (CS_INT) input_size;

	/*
	 * FIXME this fix some thing but if error cs_convert should return
	 * CS_UNUSED; note that this is defined 5.. a valid result ...
	 */
	reslen = 0;

	/* do convert */
	memset(buffer, 23, sizeof(buffer));
	check_call(cs_convert, (ctx, &srcfmt, input, &destfmt, buffer, &reslen));

	assert(reslen >= 0 && reslen < sizeof(buffer));
	buffer[reslen] = 0;

	if (strcmp(buffer, expected) != 0) {
		fprintf(stderr, "Wrong result test %s type %d:\ngot %s\nexp %s\n", test_name, type, buffer, expected);
		failed = true;
	}
}

static void
single_test(const TEST *test)
{
	/* 2025-09-07 16:32:53.490000
	 * This date was chosen for different reasons:
	 * - all components are different;
	 * - all components are not zero;
	 * - month and day are different so we can distinguish the order;
	 * - month and day are one digit so we can distinguish padding.
	 */
	CS_DATETIME date = { 45905, ((16 * 60 + 32) * 60 + 53) * 300 + 147 };
	CS_INT i_value;

	test_name = test->convfmt_name;

	i_value = test->convfmt;
	check_call(cs_dt_info, (ctx, CS_SET, NULL, CS_DT_CONVFMT, CS_UNUSED, &i_value, sizeof(i_value), NULL));

	single_value(CS_DATETIME_TYPE, &date, sizeof(date), test->datetime);
	single_value(CS_DATE_TYPE, &date.dtdays, sizeof(date.dtdays), test->date);
	single_value(CS_TIME_TYPE, &date.dttime, sizeof(date.dttime), test->time);
}

TEST_MAIN()
{
	int verbose = 1;
	const TEST *test;

	/* Force default us_enlish */
	unsetenv("LC_ALL");
	setenv("LANG", "C", 1);

	printf("%s: Testing date conversions\n", __FILE__);

	check_call(cs_ctx_alloc, (CS_VERSION_100, &ctx));

	for (test = tests; test->convfmt_name != NULL; ++test)
		single_test(test);

	check_call(cs_ctx_drop, (ctx));

	if (verbose && !failed)
		printf("Test succeded\n");

	return failed ? 1 : 0;
}
