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

static int
simple_string(void)
{
	TDSLOGIN *login;
	TDS_ERRS errs = {0};
	TDSLOCALE *locale;
	TDS_PARSED_PARAM parsed_params[ODBC_PARAM_SIZE];
	const char *connect_string = "DRIVER=libtdsodbc.so;SERVER=127.0.0.1;PORT=1337;UID=test_username;PWD=test_password;DATABASE=test_db;ClientCharset=UTF-8;";

	const char *connect_string_end = connect_string + strlen(connect_string);
	login = tds_alloc_login(0);
	if (!tds_set_language(login, "us_english"))
		return 1;
	locale = tds_alloc_locale();
	login = tds_init_login(login, locale);

	odbc_errs_reset(&errs);

	if (!odbc_parse_connect_string(&errs, connect_string, connect_string_end, login, parsed_params))
		return 1;

	assert_equal_str(parsed_params[ODBC_PARAM_UID], "test_username");
	assert_equal_dstr(login->server_name, "127.0.0.1");
	assert_equal_dstr(login->password, "test_password");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "test_password");
	assert(login->port == 1337);

	tds_free_login(login);
	tds_free_locale(locale);

	return 0;
}

static int
simple_escaped_string(void)
{
	TDSLOGIN *login;
	TDS_ERRS errs = {0};
	TDSLOCALE *locale;
	TDS_PARSED_PARAM parsed_params[ODBC_PARAM_SIZE];
	const char *connect_string = "DRIVER={libtdsodbc.so};SERVER={127.0.0.1};PORT={1337};UID={test_username};PWD={test_password};DATABASE={test_db};ClientCharset={UTF-8};";

	const char *connect_string_end = connect_string + strlen(connect_string);
	login = tds_alloc_login(0);
	if (!tds_set_language(login, "us_english"))
		return 1;
	locale = tds_alloc_locale();
	login = tds_init_login(login, locale);

	odbc_errs_reset(&errs);

	if (!odbc_parse_connect_string(&errs, connect_string, connect_string_end, login, parsed_params))
		return 1;

	assert_equal_str(parsed_params[ODBC_PARAM_UID], "{test_username}");
	assert_equal_dstr(login->server_name, "127.0.0.1");
	assert_equal_dstr(login->password, "test_password");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "{test_password}");
	assert(login->port == 1337);

	tds_free_login(login);
	tds_free_locale(locale);

	return 0;
}

static int
test_special_symbols(void)
{
	TDSLOGIN *login;
	TDS_ERRS errs = {0};
	TDSLOCALE *locale;
	TDS_PARSED_PARAM parsed_params[ODBC_PARAM_SIZE];
	const char *connect_string = "DRIVER={libtdsodbc.so};SERVER={127.0.0.1};PORT={1337};UID={test_username};PWD={[]{}}(),;?*=!@};DATABASE={test_db};ClientCharset={UTF-8};";

	const char *connect_string_end = connect_string + strlen(connect_string);
	login = tds_alloc_login(0);
	if (!tds_set_language(login, "us_english"))
		return 1;
	locale = tds_alloc_locale();
	login = tds_init_login(login, locale);

	odbc_errs_reset(&errs);

	if (!odbc_parse_connect_string(&errs, connect_string, connect_string_end, login, parsed_params))
		return 1;

	assert_equal_str(parsed_params[ODBC_PARAM_UID], "{test_username}");
	assert_equal_dstr(login->server_name, "127.0.0.1");
	assert_equal_dstr(login->password, "[]{}(),;?*=!@");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "{[]{}}(),;?*=!@}");
	assert(login->port == 1337);

	tds_free_login(login);
	tds_free_locale(locale);

	return 0;
}

static int
password_contains_curly_braces(void)
{
	TDSLOGIN *login;
	TDS_ERRS errs = {0};
	TDSLOCALE *locale;
	TDS_PARSED_PARAM parsed_params[ODBC_PARAM_SIZE];
	const char *connect_string = "DRIVER={libtdsodbc.so};SERVER={127.0.0.1};PORT={1337};UID={test_username};PWD={test{}}_password};DATABASE={test_db};ClientCharset={UTF-8};";

	const char *connect_string_end = connect_string + strlen(connect_string);
	login = tds_alloc_login(0);
	if (!tds_set_language(login, "us_english"))
		return 1;
	locale = tds_alloc_locale();
	login = tds_init_login(login, locale);

	odbc_errs_reset(&errs);

	if (!odbc_parse_connect_string(&errs, connect_string, connect_string_end, login, parsed_params))
		return 1;

	assert_equal_str(parsed_params[ODBC_PARAM_UID], "{test_username}");
	assert_equal_dstr(login->server_name, "127.0.0.1");
	assert_equal_dstr(login->password, "test{}_password");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "{test{}}_password}");
	assert(login->port == 1337);

	tds_free_login(login);
	tds_free_locale(locale);

	return 0;
}

static int
password_contains_curly_braces_and_separator(void)
{
	TDSLOGIN *login;
	TDS_ERRS errs = {0};
	TDSLOCALE *locale;
	TDS_PARSED_PARAM parsed_params[ODBC_PARAM_SIZE];
	const char *connect_string = "DRIVER={libtdsodbc.so};SERVER={127.0.0.1};PORT={1337};UID={test_username};PWD={test{}};_password};DATABASE={test_db};ClientCharset={UTF-8};";

	const char *connect_string_end = connect_string + strlen(connect_string);
	login = tds_alloc_login(0);
	if (!tds_set_language(login, "us_english"))
		return 1;
	locale = tds_alloc_locale();
	login = tds_init_login(login, locale);

	odbc_errs_reset(&errs);

	if (!odbc_parse_connect_string(&errs, connect_string, connect_string_end, login, parsed_params))
		return 1;

	assert_equal_str(parsed_params[ODBC_PARAM_UID], "{test_username}");
	assert_equal_dstr(login->server_name, "127.0.0.1");
	assert_equal_dstr(login->password, "test{};_password");
	assert_equal_str(parsed_params[ODBC_PARAM_PWD], "{test{}};_password}");
	assert(login->port == 1337);

	tds_free_login(login);
	tds_free_locale(locale);

	return 0;
}


int main(int argc, char *argv[])
{
#ifdef _WIN32
	hinstFreeTDS = GetModuleHandle(NULL);
#endif

	if (simple_string() != 0)
		return 1;

	if (simple_escaped_string() != 0)
		return 1;

	if (password_contains_curly_braces() != 0)
		return 1;

	if (test_special_symbols() != 0)
		return 1;

	if (password_contains_curly_braces_and_separator() != 0)
		return 1;

	return 0;
}
