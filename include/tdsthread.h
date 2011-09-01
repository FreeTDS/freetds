/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 *
 * Copyright (C) 2005 Liam Widdowson
 * Copyright (C) 2010 Frediano Ziglio
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

#ifndef TDSTHREAD_H
#define TDSTHREAD_H 1

/* $Id: tdsthread.h,v 1.11 2011-09-01 12:26:51 freddy77 Exp $ */

#undef TDS_HAVE_MUTEX

#if defined(_THREAD_SAFE) && defined(TDS_HAVE_PTHREAD_MUTEX)

#include <pthread.h>

#define TDS_MUTEX_DEFINE(name) pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
#define TDS_MUTEX_LOCK(mtx) pthread_mutex_lock(mtx)
#define TDS_MUTEX_TRYLOCK(mtx) pthread_mutex_trylock(mtx)
#define TDS_MUTEX_UNLOCK(mtx) do { pthread_mutex_unlock(mtx); } while(0)
#define TDS_MUTEX_DECLARE(name) pthread_mutex_t name
#define TDS_MUTEX_INIT(mtx) pthread_mutex_init(mtx, NULL)
#define TDS_MUTEX_FREE(mtx) pthread_mutex_destroy(mtx)

#define TDS_HAVE_MUTEX 1

#elif defined(_WIN32)

struct ptw32_mcs_node_t_;

typedef struct tds_win_mutex_t_ {
	struct ptw32_mcs_node_t_ *lock;
	LONG done;
	CRITICAL_SECTION crit;
} tds_win_mutex_t;

void tds_win_mutex_lock(tds_win_mutex_t *mutex);
int tds_win_mutex_trylock(tds_win_mutex_t *mutex);
static inline int tds_win_mutex_init(tds_win_mutex_t *mtx)
{
	mtx->lock = NULL;
	mtx->done = 0;
	return 0;
}
/* void tds_win_mutex_unlock(tds_win_mutex_t *mutex); */

#define TDS_MUTEX_DEFINE(name) tds_win_mutex_t name = { NULL, 0 }
#define TDS_MUTEX_LOCK(mtx) \
	do { if ((mtx)->done) EnterCriticalSection(&(mtx)->crit); else tds_win_mutex_lock(mtx); } while(0)
#define TDS_MUTEX_TRYLOCK(mtx) tds_win_mutex_trylock(mtx)
#define TDS_MUTEX_UNLOCK(mtx) LeaveCriticalSection(&(mtx)->crit)
#define TDS_MUTEX_DECLARE(name) tds_win_mutex_t name
#define TDS_MUTEX_INIT(mtx) tds_win_mutex_init(mtx)
#define TDS_MUTEX_FREE(mtx) do { if ((mtx)->done) { DeleteCriticalSection(&(mtx)->crit); (mtx)->done = 0; } } while(0)

#define TDS_HAVE_MUTEX 1

#else

#define TDS_MUTEX_DEFINE(name) int name
#define TDS_MUTEX_LOCK(mtx)
#define TDS_MUTEX_UNLOCK(mtx)
#define TDS_MUTEX_DECLARE(name) int name
#define TDS_MUTEX_INIT(mtx)
#define TDS_MUTEX_FREE(mtx)

#endif

#endif
