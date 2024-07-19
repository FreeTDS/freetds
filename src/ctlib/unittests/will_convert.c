/* Test cs_will_convert
 */
#include "common.h"

static CS_CONTEXT *context;

typedef struct {
	const char *expected;
	CS_INT from;
} test_row;

/* from
 * http://infocenter.sybase.com/help/index.jsp?topic=/com.sybase.help.ocs_12.5.1.comlib/html/comlib/X12687.htm
 * X = conversion supported
 * . = conversion not supported
 * x = should be supported, but currently it's not
 * _ = should be not supported, but currently it is
 */
static const test_row test_rows[] = {
/*
  B L V B C L V D D T S I D N F R M M B S T I U D T
  I O A I H O A A A I M N E U L E O O O E E M N A I
  N N R T A N R T T N A T C M O A N N U N X A I T M
  A G B   R G C E E Y L   I E A L E E N S T G C E E
  R B I     C H T T I L   M R T   Y Y D I   E H
  Y I N     H A I I N I   A I       4 A T     A
    N A     A R M M T N   L C         R I     R
    A R     R   E E   T               Y V
    R Y           4                     I
    Y                                   T
                                        Y
*/
{"X X X x X X X x x X X X x x X X X X . . X X X x x", CS_BINARY_TYPE},
{"X X X x X X X x x X X X x x X X X X . . X X X x x", CS_LONGBINARY_TYPE},
{"X X X x X X X x x X X X x x X X X X . . X X X x x", CS_VARBINARY_TYPE},
{"X X X X X X X . . X X X X X X X _ _ . . X X X x x", CS_BIT_TYPE},
{"X X X X X X X X X X X X X X X X X X x x X X X X X", CS_CHAR_TYPE},
{"X X X X X X X X X X X X X X X X X X x x X X X X X", CS_LONGCHAR_TYPE},
{"X X X X X X X X X X X X X X X X X X x x X X X X X", CS_VARCHAR_TYPE},
{"_ _ X . X X X X X . . . . . . . . . . . X X X X X", CS_DATETIME_TYPE},
{"_ _ X . X X X X X . . . . . . . . . . . X X X X X", CS_DATETIME4_TYPE},
{"X X X X X X X . . X X X X X X X X X . . X X _ . .", CS_TINYINT_TYPE},
{"X X X X X X X . . X X X X X X X X X . . X X _ . .", CS_SMALLINT_TYPE},
{"X X X X X X X . . X X X X X X X X X . . X X _ . .", CS_INT_TYPE},
{"X X X X X X X . . X X X X X X X X X . . X X _ . .", CS_DECIMAL_TYPE},
{"X X X X X X X . . X X X X X X X X X . . X X _ . .", CS_NUMERIC_TYPE},
{"X X X X X X X . . X X X X X X X X X . . X X _ . .", CS_FLOAT_TYPE},
{"X X X X X X X . . X X X X X X X X X . . X X _ . .", CS_REAL_TYPE},
{"X X X X X X X . . X X X X X X X X X . . X X _ . .", CS_MONEY_TYPE},
{"X X X X X X X . . X X X X X X X X X . . X X _ . .", CS_MONEY4_TYPE},
{". . . . x x x . . . . . . . . . . . x . x . . . .", CS_BOUNDARY_TYPE},
{". . . . x x x . . . . . . . . . . . . x x . . . .", CS_SENSITIVITY_TYPE},
{"X X X X X X X X X X X X X X X X X X x x X X _ _ _", CS_TEXT_TYPE},
{"X X X x X X X x x X X X x x X X X X . . X X _ . .", CS_IMAGE_TYPE},
{"X X X X X X X X X _ _ _ _ _ _ _ _ _ . . _ _ _ _ _", CS_UNICHAR_TYPE},
{"_ _ X . X X X X X . . . . . . . . . . . X X X X _", CS_DATE_TYPE},
{"_ _ X . X X X X X . . . . . . . . . . . X X X _ X", CS_TIME_TYPE},
{NULL, 0}
};

#define TEST_ALL_TYPES \
	TEST_TYPE(BINARY) \
	TEST_TYPE(LONGBINARY) \
	TEST_TYPE(VARBINARY) \
	TEST_TYPE(BIT) \
	TEST_TYPE(CHAR) \
	TEST_TYPE(LONGCHAR) \
	TEST_TYPE(VARCHAR) \
	TEST_TYPE(DATETIME) \
	TEST_TYPE(DATETIME4) \
	TEST_TYPE(TINYINT) \
	TEST_TYPE(SMALLINT) \
	TEST_TYPE(INT) \
	TEST_TYPE(DECIMAL) \
	TEST_TYPE(NUMERIC) \
	TEST_TYPE(FLOAT) \
	TEST_TYPE(REAL) \
	TEST_TYPE(MONEY) \
	TEST_TYPE(MONEY4) \
	TEST_TYPE(BOUNDARY) \
	TEST_TYPE(SENSITIVITY) \
	TEST_TYPE(TEXT) \
	TEST_TYPE(IMAGE) \
	TEST_TYPE(UNICHAR) \
	TEST_TYPE(DATE) \
	TEST_TYPE(TIME)


static CS_INT column_types[] = {
#define TEST_TYPE(type) CS_ ## type ## _TYPE,
	TEST_ALL_TYPES
#undef TEST_TYPE
	CS_ILLEGAL_TYPE
};

static const char *
type_name(CS_INT value)
{
	switch (value) {
#define TEST_TYPE(type) case CS_ ## type ## _TYPE: return #type;
	TEST_ALL_TYPES
#undef TEST_TYPE
	}
	return "unknown!";
}

static void
test(CS_INT from, CS_INT to, CS_BOOL expected)
{
	CS_BOOL res;
	res = 123;
	cs_will_convert(context, from, to, &res);
	if (res != expected) {
		fprintf(stderr, "Wrong result %d (%s) -> %d (%s) %d\n",
			from, type_name(from),
			to, type_name(to),
			res);
		exit(1);
	}
	res = 123;
	cs_will_convert(NULL, from, to, &res);
	if (res != expected) {
		fprintf(stderr, "Wrong result %d (%s) -> %d (%s) %d\n",
			from, type_name(from),
			to, type_name(to),
			res);
		exit(1);
	}
}

TEST_MAIN()
{
	int verbose = 0;
	CS_COMMAND *command;
	CS_CONNECTION *connection;
	CS_INT from, to;
	const test_row *row;

	check_call(try_ctlogin, (&context, &connection, &command, verbose));

	for (row = test_rows; row->expected; ++row) {
		const CS_INT *type;
		const char *expected = row->expected;
		for (type = column_types; *type != CS_ILLEGAL_TYPE; ++type) {
			switch (*expected) {
			case 'X':
				test(row->from, *type, CS_TRUE);
				break;
			case '.':
				test(row->from, *type, CS_FALSE);
				break;
			case 'x':
			case '_':
				/* ignore */
				break;
			default:
				assert(0);
			}
			++expected;
			assert(*expected == 0 || *expected == ' ');
			if (*expected)
				++expected;
		}
	}

	for (from = CS_MAX_SYBTYPE + 1; from < 256; ++from) {
		for (to = CS_MAX_SYBTYPE + 1; to < 256; ++to) {
			CS_BOOL res;
			res = 123;
			cs_will_convert(context, from, to, &res);
			assert(res == CS_FALSE);
			res = 123;
			cs_will_convert(NULL, from, to, &res);
			assert(res == CS_FALSE);
		}
	}

	try_ctlogout(context, connection, command, verbose);

	return 0;
}
