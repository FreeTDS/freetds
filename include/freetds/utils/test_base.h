#ifndef _tdsguard_afBM6E9n8CuIFSBHNNblq5
#define _tdsguard_afBM6E9n8CuIFSBHNNblq5

/*
 * Base header for FreeTDS unit tests, even those just covering helpers
 * from the utils and replacements trees.  Should be included first
 * (possibly via a common.h) to be certain of preceding <assert.h>.
 */

/* Ensure assert is always active. */
#undef NDEBUG
#ifdef assert
#  error "Include test_base.h (or common.h) earlier"
#endif

#include <config.h>

#include <freetds/bool.h>
#include <freetds/macros.h>

/*
 * Tests should define test_main in lieu of main so that they can be
 * configured to suppress automation-unfriendly crash dialog boxes on
 * Windows.  To that end, they can use the TEST_MAIN macro, which cleanly
 * avoids warnings for the tests that ignore their arguments (but still
 * provides the details under conventional names for the remainder).
 */
int test_main(int argc, char **argv);

#define TEST_MAIN() int test_main(int argc TDS_UNUSED, char **argv TDS_UNUSED)


typedef struct
{
	char SERVER[512];
	char DATABASE[512];
	char USER[512];
	char PASSWORD[512];
	char DRIVER[1024]; /* ODBC-only */
	char CHARSET[512];
	int  maxlength;
	bool fverbose;
	bool initialized;
	bool tried_env;
} COMMON_PWD;

extern COMMON_PWD common_pwd;

#define DEFAULT_PWD_PATH "../../../PWD"

void reset_login_info(COMMON_PWD * common_pwd);

/*
 * Both return the path used (favoring $TDSPWDFILE in the absence of
 * tried_env) on success or NULL on failure (silently in the case of
 * try_read_login_info), and defer to any existing settings from
 * e.g. the command line.
 */
const char * read_login_info_base(COMMON_PWD * common_pwd,
				  const char * default_path);
const char * try_read_login_info_base(COMMON_PWD * common_pwd,
				      const char * default_path);

#endif /* _tdsguard_afBM6E9n8CuIFSBHNNblq5 */
