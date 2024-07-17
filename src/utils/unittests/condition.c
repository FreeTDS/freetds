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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <freetds/thread.h>
#include <freetds/utils.h>
#include <freetds/macros.h>
#include <freetds/replacements.h>

#if !defined(TDS_NO_THREADSAFE)

static tds_mutex mtx = TDS_MUTEX_INITIALIZER;

static TDS_THREAD_PROC_DECLARE(signal_proc, arg)
{
	tds_condition *cond = (tds_condition *) arg;

	/* success */
	int res = 0;

	tds_mutex_lock(&mtx);
	if (tds_cond_signal(cond)) {
		/* failure */
		res = 1;
	}
	tds_mutex_unlock(&mtx);
	return TDS_THREAD_RESULT(res);
}

static void check(int cond, const char *msg)
{
	if (cond) {
		fprintf(stderr, "%s\n", msg);
		exit(1);
	}
}

TEST_MAIN()
{
	tds_condition cond;
	tds_thread th;
	void *res;

	check(tds_cond_init(&cond), "failed initializing condition");

	tds_mutex_lock(&mtx);

	check(tds_thread_create(&th, signal_proc, &cond) != 0, "error creating thread");

	tds_sleep_ms(100);

	check(tds_cond_wait(&cond, &mtx), "failed waiting condition");

	res = &th;
	check(tds_thread_join(th, &res) != 0, "error waiting thread");

	check(TDS_PTR2INT(res) != 0, "error signaling condition");

	/* under Windows mutex are recursive */
#ifndef _WIN32
	check(tds_mutex_trylock(&mtx) == 0, "mutex should be locked");
#endif

	/* check timed version */

	check(tds_cond_timedwait(&cond, &mtx, 1) != ETIMEDOUT, "should not succeed to wait condition");

	check(tds_thread_create(&th, signal_proc, &cond) != 0, "error creating thread");

	check(tds_cond_timedwait(&cond, &mtx, 1), "error on timed waiting condition");

	res = &th; /* just to avoid NULL */
	check(tds_thread_join(th, &res) != 0, "error waiting thread");

	check(TDS_PTR2INT(res) != 0, "error signaling condition");

	check(tds_thread_create(&th, signal_proc, &cond) != 0, "error creating thread");

	check(tds_cond_timedwait(&cond, &mtx, -1), "error on timed waiting condition");

	res = &th; /* just to avoid NULL */
	check(tds_thread_join(th, &res) != 0, "error waiting thread");

	check(TDS_PTR2INT(res) != 0, "error signaling condition");

	check(tds_thread_create(&th, signal_proc, &cond) != 0, "error creating thread");

	check(tds_cond_timedwait(&cond, &mtx, 0), "error on timed waiting condition");

	res = &th; /* just to avoid NULL */
	check(tds_thread_join(th, &res) != 0, "error waiting thread");

	check(TDS_PTR2INT(res) != 0, "error signaling condition");

	tds_mutex_unlock(&mtx);

	check(tds_cond_destroy(&cond), "failed destroying condition");
	return 0;
}

#else

TEST_MAIN()
{
	return 0;
}

#endif

