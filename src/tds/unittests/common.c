#include <stdio.h>
#include <string.h>
#include <tds.h>
#include "common.h"

static char software_version[] = "$Id: common.c,v 1.9 2002-11-20 13:34:49 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

char USER[512];
char SERVER[512];
char PASSWORD[512];
char DATABASE[512];

int read_login_info(void);

int
read_login_info(void)
{
	FILE *in;
	char line[512];
	char *s1, *s2;

	in = fopen("../../../PWD", "r");
	if (!in) {
		fprintf(stderr, "Can not open PWD file\n\n");
		return TDS_FAIL;
	}

	while (fgets(line, 512, in)) {
		s1 = strtok(line, "=");
		s2 = strtok(NULL, "\n");
		if (!s1 || !s2) {
			continue;
		}
		if (!strcmp(s1, "UID")) {
			strcpy(USER, s2);
		} else if (!strcmp(s1, "SRV")) {
			strcpy(SERVER, s2);
		} else if (!strcmp(s1, "PWD")) {
			strcpy(PASSWORD, s2);
		} else if (!strcmp(s1, "DB")) {
			strcpy(DATABASE, s2);
		}
	}
	return TDS_SUCCEED;
}


int
try_tds_login(TDSLOGIN ** login, TDSSOCKET ** tds, const char *appname, int verbose)
{
	TDSCONTEXT *context;
	TDSCONNECTINFO *connect_info;

	if (verbose) {
		fprintf(stdout, "Entered tds_try_login()\n");
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
		fprintf(stdout, "Trying read_login_info()\n");
	}
	read_login_info();

	if (verbose) {
		fprintf(stdout, "Setting login parameters\n");
	}
	*login = tds_alloc_login();
	if (!*login) {
		fprintf(stderr, "tds_alloc_login() failed.\n");
		return TDS_FAIL;
	}
	tds_set_passwd(*login, PASSWORD);
	tds_set_user(*login, USER);
	tds_set_app(*login, appname);
	tds_set_host(*login, "myhost");
	tds_set_library(*login, "TDS-Library");
	tds_set_server(*login, SERVER);
	tds_set_charset(*login, "iso_1");
	tds_set_language(*login, "us_english");
	tds_set_packet(*login, 512);

	if (verbose) {
		fprintf(stdout, "Connecting to database\n");
	}
	context = tds_alloc_context();
	*tds = tds_alloc_socket(context, 512);
	tds_set_parent(*tds, NULL);
	connect_info = tds_read_config_info(NULL, *login, context->locale);
	if (!connect_info || tds_connect(*tds, connect_info) == TDS_FAIL) {
		tds_free_connect(connect_info);
		fprintf(stderr, "tds_connect() failed\n");
		return TDS_FAIL;
	}
	tds_free_connect(connect_info);

	return TDS_SUCCEED;
}


/* Note that this always suceeds */
int
try_tds_logout(TDSLOGIN * login, TDSSOCKET * tds, int verbose)
{
	if (verbose) {
		fprintf(stdout, "Entered tds_try_logout()\n");
	}
	tds_free_socket(tds);
	tds_free_login(login);
	return TDS_SUCCEED;
}
