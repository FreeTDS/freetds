#include "common.h"
#include <assert.h>
#include "freetds/odbc.h"


#ifdef _WIN32
HINSTANCE hinstFreeTDS;
#endif

static void
assert_equal_dstr(DSTR a, const char *b)
{
	assert(b && strcmp(tds_dstr_cstr(&a), b)==0);
}

static void
assert_equal_str(TDS_PARSED_PARAM param, const char *b)
{
	/* printf("param %.*s b %s\n", (int) param.len, param.p, b); */
	assert(b && strlen(b) == param.len && strncmp(param.p, b, param.len)==0);
}

typedef void check_func_t(TDSLOGIN *login, TDS_PARSED_PARAM *parsed_params);

static void
test_common(const char *name, const char *connect_string, check_func_t *check_func)
{
	TDSLOGIN *login;
	TDS_ERRS errs = {0};
	TDSLOCALE *locale;
	TDS_PARSED_PARAM parsed_params[ODBC_PARAM_SIZE];

	const char *connect_string_end = connect_string + strlen(connect_string);
	login = tds_alloc_login(false);
	if (!tds_set_language(login, "us_english")) {
		fprintf(stderr, "Error setting language in test '%s'\n", name);
		exit(1);
	}
	locale = tds_alloc_locale();
	login = tds_init_login(login, locale);

	odbc_errs_reset(&errs);

	if (!odbc_parse_connect_string(&errs, connect_string, connect_string_end, login, parsed_params)) {
		fprintf(stderr, "Error parsing string in test '%s'\n", name);
		exit(1);
	}

	check_func(login, parsed_params);

	tds_free_login(login);
	tds_free_locale(locale);
}

#define CHECK(name, s) \
	static const char *name ## _connect_string = s; \
	static void name ## _check(TDSLOGIN *login, TDS_PARSED_PARAM *parsed_params); \
	static void name(void) { \
		test_common(#name, name ## _connect_string, name ## _check); \
	} \
	static void name ## _check(TDSLOGIN *login, TDS_PARSED_PARAM *parsed_params)

CHECK(simple_string,
	"DRIVER=libtdsodbc.so;SERVER=127.0.0.1;PORT=1337;UID=test_username;PWD=test_password;DATABASE=test_db;ClientCharset=UTF-8;")
{
	assert_equal_str(parsed_params[ODBC_PARAM_UID], "test_username");
	assert_equal_dstr(login->server_name, "127.0.0.1");
	assert_equal_dstr(login->password, "test_password");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "test_password");
	assert(login->port == 1337);
}

CHECK(simple_escaped_string,
	"DRIVER={libtdsodbc.so};SERVER={127.0.0.1};PORT={1337};UID={test_username};PWD={test_password};DATABASE={test_db};ClientCharset={UTF-8};")
{
	assert_equal_str(parsed_params[ODBC_PARAM_UID], "{test_username}");
	assert_equal_dstr(login->server_name, "127.0.0.1");
	assert_equal_dstr(login->password, "test_password");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "{test_password}");
	assert(login->port == 1337);
}

CHECK(test_special_symbols,
	"DRIVER={libtdsodbc.so};SERVER={127.0.0.1};PORT={1337};UID={test_username};PWD={[]{}}(),;?*=!@};DATABASE={test_db};ClientCharset={UTF-8};")
{
	assert_equal_str(parsed_params[ODBC_PARAM_UID], "{test_username}");
	assert_equal_dstr(login->server_name, "127.0.0.1");
	assert_equal_dstr(login->password, "[]{}(),;?*=!@");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "{[]{}}(),;?*=!@}");
	assert(login->port == 1337);
}

CHECK(password_contains_curly_braces,
	"DRIVER={libtdsodbc.so};SERVER={127.0.0.1};PORT={1337};UID={test_username};PWD={test{}}_password};DATABASE={test_db};ClientCharset={UTF-8};")
{
	assert_equal_str(parsed_params[ODBC_PARAM_UID], "{test_username}");
	assert_equal_dstr(login->server_name, "127.0.0.1");
	assert_equal_dstr(login->password, "test{}_password");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "{test{}}_password}");
	assert(login->port == 1337);
}

CHECK(password_contains_curly_braces_and_separator,
	"DRIVER={libtdsodbc.so};SERVER={127.0.0.1};PORT={1337};UID={test_username};PWD={test{}};_password};DATABASE={test_db};ClientCharset={UTF-8};")
{
	assert_equal_str(parsed_params[ODBC_PARAM_UID], "{test_username}");
	assert_equal_dstr(login->server_name, "127.0.0.1");
	assert_equal_dstr(login->password, "test{};_password");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "{test{}};_password}");
	assert(login->port == 1337);
}

CHECK(password_bug_report,
	"Driver=FreeTDS;Server=1.2.3.4;Port=1433;Database=test;uid=test_user;pwd={p@ssw0rd}")
{
	assert_equal_str(parsed_params[ODBC_PARAM_UID], "test_user");
	assert_equal_dstr(login->server_name, "1.2.3.4");
	assert_equal_dstr(login->password, "p@ssw0rd");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "{p@ssw0rd}");
	assert(login->port == 1433);
}

int
main(void)
{
#ifdef _WIN32
	hinstFreeTDS = GetModuleHandle(NULL);
#endif

	simple_string();

	simple_escaped_string();

	password_contains_curly_braces();

	test_special_symbols();

	password_contains_curly_braces_and_separator();

	password_bug_report();

	return 0;
}
