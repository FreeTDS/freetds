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

static char software_version[] = "$Id: t0001.c,v 1.6 2003-04-21 16:06:10 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char **argv)
{
	TDSLOGIN *login;
	TDSSOCKET *tds;
	int ret;
	int verbose = 0;

	fprintf(stdout, "%s: Testing login, logout\n", __FILE__);
	ret = try_tds_login(&login, &tds, __FILE__, verbose);
	if (ret != TDS_SUCCEED) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}

	try_tds_logout(login, tds, verbose);
	return 0;
}
