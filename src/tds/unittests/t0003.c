/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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

static char software_version[] = "$Id: t0003.c,v 1.11 2003-06-03 06:26:38 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };


int
main(int argc, char **argv)
{
	TDSLOGIN *login;
	TDSSOCKET *tds;
	int verbose = 0;
	int rc;
	TDS_INT result_type;

	fprintf(stdout, "%s: Testing DB change -- 'use tempdb'\n", __FILE__);
	rc = try_tds_login(&login, &tds, __FILE__, verbose);
	if (rc != TDS_SUCCEED) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}

	rc = tds_submit_query(tds, "use tempdb", NULL);
	if (rc != TDS_SUCCEED) {
		fprintf(stderr, "tds_submit_query() failed\n");
		return 1;
	}

	/* warning: this mucks with some internals to get the env chg message */
	if (tds_process_simple_query(tds, &result_type) == TDS_FAIL || result_type == TDS_CMD_FAIL) {
		fprintf(stderr, "query results failed\n");
		return 1;
	}

	if (!tds || !tds->env || !tds->env->database) {
		fprintf(stderr, "No database ??\n");
		return 1;
	}

	/* Test currently disabled during TDSENVINFO changes */
	if (verbose) {
		fprintf(stdout, "database changed to %s\n", tds->env->database);
	}
	if (strcmp(tds->env->database, "tempdb")) {
		fprintf(stderr, "Wrong database, %s != tempdb\n", tds->env->database);
		return 1;
	}

	try_tds_logout(login, tds, verbose);
	return 0;
}
