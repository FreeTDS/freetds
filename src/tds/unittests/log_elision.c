/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2021  Frediano Ziglio
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
 * Check log elision implementation
 */
#include "common.h"
#include <assert.h>
#include <freetds/utils.h>

#if HAVE_UNISTD_H
#undef getpid
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef TDS_HAVE_MUTEX
enum {
	LOOP = 100,
	THREADS = 3,
};

static TDS_THREAD_PROC_DECLARE(log_func, idx_ptr)
{
	const int idx = TDS_PTR2INT(idx_ptr);
	const char letter = 'A' + idx;
	TDSDUMP_OFF_ITEM off_item;
	int n = idx, i;

	/* LOOP times */
	for (i = 0; i < LOOP; ++i) {
		/* send log */
		tdsdump_log(TDS_DBG_ERROR, "Some log from %c number %d\n", letter, n);

		/* wait 1-10 ms */
		tds_sleep_ms((rand() % 10) + 1);

		/* disable logs */
		tdsdump_off(&off_item);

		/* send wrong log */
		tdsdump_log(TDS_DBG_ERROR, "Disabled log %c number %d\n", letter, n);

		/* wait 1-10 ms */
		tds_sleep_ms((rand() % 10) + 1);

		/* enable logs */
		tdsdump_on(&off_item);

		n += 3;
	}

	return TDS_THREAD_RESULT(0);
}

TEST_MAIN()
{
	int i, ret;
	tds_thread threads[THREADS];
	FILE *f;
	char line[1024];
	int wrong_lines = 0;
	int nexts[THREADS];

	tds_debug_flags = TDS_DBGFLAG_ALL | TDS_DBGFLAG_SOURCE;

	/* remove file */
	unlink("log_elision.out");

	/* set output file */
	tdsdump_open(TDS_DIR("log_elision.out"));

	/* THREADS thread */
	for (i = 0; i < THREADS; ++i) {
		nexts[i] = i;
	}
	for (i = 1; i < THREADS; ++i) {
		ret = tds_thread_create(&threads[i], log_func, TDS_INT2PTR(i));
		assert(ret == 0);
	}
	log_func(TDS_INT2PTR(0));
	for (i = 1; i < THREADS; ++i) {
		ret = tds_thread_join(threads[i], NULL);
		assert(ret == 0);
	}

	/* close logs */
	tdsdump_close();

	/* open logs to read */
	f = fopen("log_elision.out", "r");
	assert(f != NULL);

	/* read line by line */
	while (fgets(line, sizeof(line), f) != NULL) {
		char thread_letter;
		int num, idx;
		char *start;

		/* ignore some start lines */
		if (strstr(line, "log_elision.c") == NULL) {
			assert(++wrong_lines < 4);
			continue;
		}

		start = strstr(line, ":Some log from");
		assert(start != NULL);

		ret = sscanf(start, ":Some log from %c number %d\n",
			     &thread_letter, &num);
		assert(ret == 2);

		/* detect number of thread */
		assert(thread_letter >= 'A' && thread_letter < 'A' + THREADS);
		idx = thread_letter - 'A';

		/* check number inside string match the next */
		assert(num == nexts[idx]);
		nexts[idx] += 3;
	}
	fclose(f);
	f = NULL;

	/* check we got all numbers */
	for (i = 0; i < THREADS; ++i) {
		assert(nexts[i] == i + LOOP * 3);
	}

	/* cleanup file */
	unlink("log_elision.out");

	return 0;
}
#else	/* !TDS_HAVE_MUTEX */
TEST_MAIN()
{
	printf("Not possible for this platform.\n");
	return 0; /* TODO 77 ? */
}
#endif
