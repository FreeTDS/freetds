/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2012  Frediano Ziglio
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

#include <config.h>

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "tdsthread.h"

static TDS_MUTEX_DEFINE(mtx);
typedef TDS_MUTEX_DECLARE(tds_mutex_t);

static void test(tds_mutex_t *mtx)
{
	if (TDS_MUTEX_TRYLOCK(mtx)) {
		fprintf(stderr, "mutex should be unlocked\n");
		exit(1);
	}
	if (!TDS_MUTEX_TRYLOCK(mtx)) {
		fprintf(stderr, "mutex should be locked\n");
		exit(1);
	}
	TDS_MUTEX_UNLOCK(mtx);
}

int main()
{
	TDS_MUTEX_DECLARE(local);

	test(&mtx);

	/* try allocating it */
	if (TDS_MUTEX_INIT(&local)) {
		fprintf(stderr, "error creating mutex\n");
		exit(1);
	}
	test(&local);
	TDS_MUTEX_FREE(&local);

	/* try again */
	if (TDS_MUTEX_INIT(&local)) {
		fprintf(stderr, "error creating mutex\n");
		exit(1);
	}
	test(&local);
	TDS_MUTEX_FREE(&local);

	return 0;
}

