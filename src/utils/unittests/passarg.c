/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2014  Frediano Ziglio
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

#include <freetds/utils/test_base.h>

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <assert.h>

#include <freetds/sysdep_private.h>
#include <freetds/utils.h>

TEST_MAIN()
{
	FILE *f;
	char *pwd = strdup("password");
	char *p;

	p = tds_getpassarg(pwd);
	assert(p);
	assert(strcmp(pwd, "********") == 0);
	assert(strcmp(p, "password") == 0);
	free(p);
	free(pwd);

	f = fopen("passarg.in", "w");
	assert(f);
	fputs("line1pwd\nline2pwd\n", f);
	fclose(f);

	f = freopen("passarg.in", "r", stdin);
	assert(f);

	p = tds_getpassarg("-");
	assert(p);
	assert(strcmp(p, "line1pwd") == 0);
	free(p);

	unlink("passarg.in");

	return 0;
}

