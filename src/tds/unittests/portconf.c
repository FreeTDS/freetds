/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2019  Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "common.h"
#include <freetds/replacements.h>

static void
set_interface(void)
{
	const char *in_file = FREETDS_SRCDIR "/portconf.in";

	FILE *f = fopen(in_file, "r");
	if (!f) {
		in_file = "portconf.in";
		f = fopen(in_file, "r");
	}
	if (!f) {
		fprintf(stderr, "error opening test file\n");
		exit(1);
	}
	fclose(f);
	tds_set_interfaces_file_loc(in_file);
}

static void
dump_login(const char *name, const TDSLOGIN *login)
{
	printf("Dump login %s\n", name);
#define STR(name) printf(" " #name ": %s\n", tds_dstr_cstr(&login->name));
#define INT(name) printf(" " #name ": %d\n", login->name);
	STR(server_name);
	STR(server_host_name);
	STR(instance_name);
	INT(port);
}

static void
test0(TDSCONTEXT *ctx, TDSSOCKET *tds, const char *input, const char *expected, int line)
{
	TDSLOGIN *login, *connection;
	char *ret = NULL;

	login = tds_alloc_login(true);
	if (!login || !tds_set_server(login, input)) {
		fprintf(stderr, "Error setting login!\n");
		exit(1);
	}

	connection = tds_read_config_info(tds, login, ctx->locale);
	dump_login("input", login);
	dump_login("final", connection);
	if (asprintf(&ret, "%s,%s,%s,%d",
		     tds_dstr_cstr(&connection->server_name),
		     tds_dstr_cstr(&connection->server_host_name),
		     tds_dstr_cstr(&connection->instance_name),
		     connection->port) < 0)
		exit(1);
	if (strcmp(ret, expected) != 0) {
		fprintf(stderr, "Mismatch line %d:\n  OUT: %s\n  EXP: %s\n",
			line, ret, expected);
		exit(1);
	}
	tds_free_login(connection);
	tds_free_login(login);
	free(ret);
}
#define test(in, out) test0(ctx, tds, in, out, __LINE__)

TEST_MAIN()
{
	TDSCONTEXT *ctx = tds_alloc_context(NULL);
	TDSSOCKET *tds = tds_alloc_socket(ctx, 512);
	FILE *f;

	/* set an empty base configuration */
	f = fopen("empty.conf", "w");
	if (f)
		fclose(f);
	putenv("FREETDSCONF=empty.conf");
	unsetenv("TDSPORT");

	set_interface();
	if (!ctx || !tds) {
		fprintf(stderr, "Error creating socket!\n");
		return 1;
	}

	test("NotExistingServer1:1234", "my_server,8.7.6.5,,1234");
	test("localhost:1234", "localhost,localhost,,1234");
	test("NotExistingServer1\\named", "my_server,8.7.6.5,named,0");
	test("localhost\\named", "localhost,localhost,named,0");
	test("2.3.4.5:2345", "2.3.4.5,2.3.4.5,,2345");
	test("[2::3]:432", "2::3,2::3,,432");
	test("[2::3:4]\\instance", "2::3:4,2::3:4,instance,0");

	tds_free_socket(tds);
	tds_free_context(ctx);
	tds_set_interfaces_file_loc(NULL);

	return 0;
}

