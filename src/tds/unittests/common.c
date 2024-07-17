#define TDS_DONT_DEFINE_DEFAULT_FUNCTIONS
#include "common.h"
#include <freetds/replacements.h>

int read_login_info(void);

int
read_login_info(void)
{
	const char *s;

	s = read_login_info_base(&common_pwd, DEFAULT_PWD_PATH);
	return s ? TDS_SUCCESS : TDS_FAIL;
}

TDSCONTEXT *test_context = NULL;

int
try_tds_login(TDSLOGIN ** login, TDSSOCKET ** tds, const char *appname, int verbose)
{
	TDSLOGIN *connection;
	char *appname_copy;
	const char *charset = "ISO-8859-1";

	if (verbose) {
		printf("Entered tds_try_login()\n");
	}
	if (!login) {
		fprintf(stderr, "Invalid TDSLOGIN**\n");
		return TDS_FAIL;
	}
	if (!tds) {
		fprintf(stderr, "Invalid TDSSOCKET**\n");
		return TDS_FAIL;
	}

	if (verbose) {
		printf("Trying read_login_info()\n");
	}
	read_login_info();

	if (verbose) {
		printf("Setting login parameters\n");
	}
	*login = tds_alloc_login(true);
	if (!*login) {
		fprintf(stderr, "tds_alloc_login() failed.\n");
		return TDS_FAIL;
	}
	appname_copy = strdup(appname);
	if (common_pwd.charset[0])
		charset = common_pwd.charset;
	if (!tds_set_passwd(*login, common_pwd.password)
	    || !tds_set_user(*login, common_pwd.user)
	    || !appname_copy
	    || !tds_set_app(*login, basename(appname_copy))
	    || !tds_set_host(*login, "myhost")
	    || !tds_set_library(*login, "TDS-Library")
	    || !tds_set_server(*login, common_pwd.server)
	    || !tds_set_client_charset(*login, charset)
	    || !tds_set_language(*login, "us_english")) {
		free(appname_copy);
		fprintf(stderr, "tds_alloc_login() failed.\n");
		return TDS_FAIL;
	}
	free(appname_copy);

	if (verbose) {
		printf("Connecting to database\n");
	}
	test_context = tds_alloc_context(NULL);
	*tds = tds_alloc_socket(test_context, 512);
	tds_set_parent(*tds, NULL);
	connection = tds_read_config_info(*tds, *login, test_context->locale);
	if (!connection || tds_connect_and_login(*tds, connection) != TDS_SUCCESS) {
		if (connection) {
			tds_free_socket(*tds);
			*tds = NULL;
			tds_free_login(connection);
		}
		fprintf(stderr, "tds_connect_and_login() failed\n");
		return TDS_FAIL;
	}
	tds_free_login(connection);

	return TDS_SUCCESS;
}


/* Note that this always suceeds */
int
try_tds_logout(TDSLOGIN * login, TDSSOCKET * tds, int verbose)
{
	if (verbose) {
		printf("Entered tds_try_logout()\n");
	}
	tds_close_socket(tds);
	tds_free_socket(tds);
	tds_free_login(login);
	tds_free_context(test_context);
	test_context = NULL;
	return TDS_SUCCESS;
}

/* Run query for which there should be no return results */
int
run_query(TDSSOCKET * tds, const char *query)
{
	int rc;
	int result_type;

	rc = tds_submit_query(tds, query);
	if (rc != TDS_SUCCESS) {
		fprintf(stderr, "tds_submit_query() failed for query '%s'\n", query);
		return TDS_FAIL;
	}

	while ((rc = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS)) == TDS_SUCCESS) {

		switch (result_type) {
		case TDS_DONE_RESULT:
		case TDS_DONEPROC_RESULT:
		case TDS_DONEINPROC_RESULT:
			/* ignore possible spurious result (TDS7+ send it) */
		case TDS_STATUS_RESULT:
			break;
		default:
			fprintf(stderr, "Error:  query should not return results\n");
			return TDS_FAIL;
		}
	}
	if (rc == TDS_FAIL) {
		fprintf(stderr, "tds_process_tokens() returned TDS_FAIL for '%s'\n", query);
		return TDS_FAIL;
	} else if (rc != TDS_NO_MORE_RESULTS) {
		fprintf(stderr, "tds_process_tokens() unexpected return\n");
		return TDS_FAIL;
	}

	return TDS_SUCCESS;
}
