/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2026  Frediano Ziglio
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

/*
 * Purpose: test tds_get_spn, computing service principal names.
 * Host names contain a dot, so no name resolution is done.
 */
#include "common.h"
#include <assert.h>

static TDSSOCKET *tds;

static void
check_spn(const char *expected)
{
	char *spn = tds_get_spn(tds);

	if (!spn || strcmp(spn, expected) != 0) {
		fprintf(stderr, "Wrong SPN: got '%s' expected '%s'\n", spn ? spn : "(null)", expected);
		exit(1);
	}
	free(spn);
}

TEST_MAIN()
{
	TDSCONTEXT *ctx = tds_alloc_context(NULL);
	TDSLOGIN *login = tds_alloc_login(false);

	tds = tds_alloc_socket(ctx, 512);
	assert(ctx && login && tds);
	tds->login = login;
	tds->conn->tds_version = 0x704;

	assert(tds_dstr_copy(&login->server_name, "MYSERVER"));
	assert(tds_dstr_copy(&login->server_host_name, "db.example.com"));
	login->port = 1433;

	/* port */
	check_spn("MSSQLSvc/db.example.com:1433");

	/* instance name is used instead of port */
	assert(tds_dstr_copy(&login->instance_name, "SQL2022"));
	check_spn("MSSQLSvc/db.example.com:SQL2022");

	/* IP address is used as is */
	assert(tds_dstr_copy(&login->server_host_name, "192.168.1.2"));
	tds_dstr_empty(&login->instance_name);
	check_spn("MSSQLSvc/192.168.1.2:1433");

	/* configured SPN */
	assert(tds_dstr_copy(&login->server_spn, "MSSQLSvc/other.example.com:1500"));
	check_spn("MSSQLSvc/other.example.com:1500");
	tds_dstr_empty(&login->server_spn);

	/* Sybase uses server name */
	tds->conn->tds_version = 0x500;
	check_spn("MYSERVER");

	tds->login = NULL;
	tds_free_login(login);
	tds_free_socket(tds);
	tds_free_context(ctx);
	return 0;
}
