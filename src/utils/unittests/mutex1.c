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

#include <freetds/utils/test_base.h>

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <freetds/thread.h>
#include <freetds/macros.h>

#if !defined(TDS_NO_THREADSAFE)

static tds_mutex mtx = TDS_MUTEX_INITIALIZER;

static TDS_THREAD_PROC_DECLARE(trylock_proc, arg)
{
	tds_mutex *mtx = (tds_mutex *) arg;

	if (!tds_mutex_trylock(mtx)) {
		/* got mutex, failure as should be locked */
		return TDS_THREAD_RESULT(1);
	}
	/* success */
	return TDS_THREAD_RESULT(0);
}

static void
test(tds_mutex *mtx)
{
	tds_thread th;
	void *res;

	if (tds_mutex_trylock(mtx)) {
		fprintf(stderr, "mutex should be unlocked\n");
		exit(1);
	}
	/* check mutex are not recursive, even on Windows */
	if (!tds_mutex_trylock(mtx)) {
		fprintf(stderr, "mutex should be locked\n");
		exit(1);
	}

	if (tds_thread_create(&th, trylock_proc, mtx) != 0) {
		fprintf(stderr, "error creating thread\n");
		exit(1);
	}

	if (tds_thread_join(th, &res) != 0) {
		fprintf(stderr, "error waiting thread\n");
		exit(1);
	}

	if (TDS_PTR2INT(res) != 0) {
		fprintf(stderr, "mutex should be locked inside thread\n");
		exit(1);
	}

	tds_mutex_unlock(mtx);

	/* now trylock after the unlock should succeed */
	if (tds_mutex_trylock(mtx)) {
		fprintf(stderr, "mutex should be unlocked\n");
		exit(1);
	}

	tds_mutex_unlock(mtx);
}

TEST_MAIN()
{
	tds_mutex local;

	test(&mtx);

	/* try allocating it */
	if (tds_mutex_init(&local)) {
		fprintf(stderr, "error creating mutex\n");
		exit(1);
	}
	test(&local);
	tds_mutex_free(&local);

	/* try again */
	if (tds_mutex_init(&local)) {
		fprintf(stderr, "error creating mutex\n");
		exit(1);
	}
	test(&local);
	tds_mutex_free(&local);

	return 0;
}

#else

TEST_MAIN()
{
	return 0;
}

#endif

