/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 *
 * Copyright (C) 2005 Liam Widdowson
 * Copyright (C) 2010-2012 Frediano Ziglio
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

#ifndef _tdsguard_cIfZP7JZiHtLLfanwl7ubP_
#define _tdsguard_cIfZP7JZiHtLLfanwl7ubP_

#undef TDS_HAVE_MUTEX

#if defined(_THREAD_SAFE) && defined(TDS_HAVE_PTHREAD_MUTEX)

#include <tds_sysdep_public.h>
#include <freetds/sysdep_private.h>
#include <pthread.h>
#include <errno.h>

#include <freetds/pushvis.h>

typedef pthread_mutex_t tds_raw_mutex;
#define TDS_RAW_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

static inline void tds_raw_mutex_lock(tds_raw_mutex *mtx)
{
	pthread_mutex_lock(mtx);
}

static inline int tds_raw_mutex_trylock(tds_raw_mutex *mtx)
{
	return pthread_mutex_trylock(mtx);
}

static inline void tds_raw_mutex_unlock(tds_raw_mutex *mtx)
{
	pthread_mutex_unlock(mtx);
}

static inline int tds_raw_mutex_init(tds_raw_mutex *mtx)
{
	return pthread_mutex_init(mtx, NULL);
}

static inline void tds_raw_mutex_free(tds_raw_mutex *mtx)
{
	pthread_mutex_destroy(mtx);
}

typedef pthread_cond_t tds_condition;

int tds_raw_cond_init(tds_condition *cond);
static inline int tds_raw_cond_destroy(tds_condition *cond)
{
	return pthread_cond_destroy(cond);
}
static inline int tds_raw_cond_signal(tds_condition *cond)
{
	return pthread_cond_signal(cond);
}
static inline int tds_raw_cond_wait(tds_condition *cond, tds_raw_mutex *mtx)
{
	return pthread_cond_wait(cond, mtx);
}
int tds_raw_cond_timedwait(tds_condition *cond, tds_raw_mutex *mtx, int timeout_sec);

#define TDS_HAVE_MUTEX 1

typedef pthread_t tds_thread;
typedef pthread_t tds_thread_id;
typedef void *(*tds_thread_proc)(void *arg);
#define TDS_THREAD_PROC_DECLARE(name, arg) \
	void *name(void *arg)
#define TDS_THREAD_RESULT(n) ((void*)(TDS_INTPTR)(n))

static inline int tds_thread_create(tds_thread *ret, tds_thread_proc proc, void *arg)
{
	return pthread_create(ret, NULL, proc, arg);
}

static inline int tds_thread_create_detached(tds_thread_proc proc, void *arg)
{
	tds_thread th;
	int ret = pthread_create(&th, NULL, proc, arg);
	if (!ret)
		pthread_detach(th);
	return ret;
}

static inline int tds_thread_join(tds_thread th, void **ret)
{
	return pthread_join(th, ret);
}

static inline tds_thread_id tds_thread_get_current_id(void)
{
	return pthread_self();
}

static inline int tds_thread_is_current(tds_thread_id th)
{
	return pthread_equal(th, pthread_self());
}

#include <freetds/popvis.h>

#elif defined(_WIN32)

#include <freetds/windows.h>
#include <errno.h>

/* old version of Windows do not define this constant */
#ifndef ETIMEDOUT
#define ETIMEDOUT 138
#endif

struct ptw32_mcs_node_t_;

typedef struct {
	struct ptw32_mcs_node_t_ *lock;
	LONG done;
	DWORD thread_id;
	CRITICAL_SECTION crit;
} tds_raw_mutex;

#define TDS_RAW_MUTEX_INITIALIZER { NULL, 0, 0 }

static inline int
tds_raw_mutex_init(tds_raw_mutex *mtx)
{
	mtx->lock = NULL;
	mtx->done = 0;
	mtx->thread_id = 0;
	return 0;
}

void tds_win_mutex_lock(tds_raw_mutex *mutex);

static inline void tds_raw_mutex_lock(tds_raw_mutex *mtx)
{
	if (mtx->done) {
		EnterCriticalSection(&mtx->crit);
		mtx->thread_id = GetCurrentThreadId();
	} else {
		tds_win_mutex_lock(mtx);
	}
}

int tds_raw_mutex_trylock(tds_raw_mutex *mtx);

static inline void tds_raw_mutex_unlock(tds_raw_mutex *mtx)
{
	mtx->thread_id = 0;
	LeaveCriticalSection(&mtx->crit);
}

static inline void tds_raw_mutex_free(tds_raw_mutex *mtx)
{
	if (mtx->done) {
		DeleteCriticalSection(&mtx->crit);
		mtx->done = 0;
	}
}

#define TDS_HAVE_MUTEX 1

/* easy way, only single signal supported */
typedef void *TDS_CONDITION_VARIABLE;
typedef union {
	HANDLE ev;
	TDS_CONDITION_VARIABLE cv;
} tds_condition;

extern int (*tds_raw_cond_init)(tds_condition *cond);
extern int (*tds_raw_cond_destroy)(tds_condition *cond);
extern int (*tds_raw_cond_signal)(tds_condition *cond);
extern int (*tds_raw_cond_timedwait)(tds_condition *cond, tds_raw_mutex *mtx, int timeout_sec);
static inline int tds_raw_cond_wait(tds_condition *cond, tds_raw_mutex *mtx)
{
	return tds_raw_cond_timedwait(cond, mtx, -1);
}

typedef HANDLE tds_thread;
typedef DWORD  tds_thread_id;
typedef DWORD (WINAPI *tds_thread_proc)(void *arg);
#define TDS_THREAD_PROC_DECLARE(name, arg) \
	DWORD WINAPI name(void *arg)
#define TDS_THREAD_RESULT(n) ((DWORD)(int)(n))

static inline int tds_thread_create(tds_thread *ret, tds_thread_proc proc, void *arg)
{
	*ret = CreateThread(NULL, 0, proc, arg, 0, NULL);
	return *ret != NULL ? 0 : 11 /* EAGAIN */;
}

static inline int tds_thread_create_detached(tds_thread_proc proc, void *arg)
{
	HANDLE h = CreateThread(NULL, 0, proc, arg, 0, NULL);
	if (h)
		return 0;
	CloseHandle(h);
	return 11 /* EAGAIN */;
}

static inline int tds_thread_join(tds_thread th, void **ret)
{
	if (WaitForSingleObject(th, INFINITE) == WAIT_OBJECT_0) {
		if (ret) {
			DWORD r;
			if (!GetExitCodeThread(th, &r))
				r = 0xffffffffu;
			*ret = (void*) (((char*)0) + r);
		}

		CloseHandle(th);
		return 0;
	}
	CloseHandle(th);
	return 22 /* EINVAL */;
}

static inline tds_thread_id tds_thread_get_current_id(void)
{
	return GetCurrentThreadId();
}

static inline int tds_thread_is_current(tds_thread_id th)
{
	return th == GetCurrentThreadId();
}

#else

#include <tds_sysdep_public.h>

/* define noops as "successful" */
typedef struct {
	char dummy[0]; /* compiler compatibility */
} tds_raw_mutex;

#define TDS_RAW_MUTEX_INITIALIZER {}

static inline void tds_raw_mutex_lock(tds_raw_mutex *mtx)
{
}

static inline int tds_raw_mutex_trylock(tds_raw_mutex *mtx)
{
	return 0;
}

static inline void tds_raw_mutex_unlock(tds_raw_mutex *mtx)
{
}

static inline int tds_raw_mutex_init(tds_raw_mutex *mtx)
{
	return 0;
}

static inline void tds_raw_mutex_free(tds_raw_mutex *mtx)
{
}

typedef struct {
	char dummy[0]; /* compiler compatibility */
} tds_condition;

static inline int tds_raw_cond_init(tds_condition *cond)
{
	return 0;
}
static inline int tds_raw_cond_destroy(tds_condition *cond)
{
	return 0;
}
#define tds_raw_cond_signal(cond) \
	FreeTDS_Condition_not_compiled

#define tds_raw_cond_wait(cond, mtx) \
	FreeTDS_Condition_not_compiled

#define tds_raw_cond_timedwait(cond, mtx, timeout_sec) \
	FreeTDS_Condition_not_compiled

typedef struct {
	char dummy[0]; /* compiler compatibility */
} tds_thread;
typedef int tds_thread_id;

typedef void *(*tds_thread_proc)(void *arg);
#define TDS_THREAD_PROC_DECLARE(name, arg) \
	void *name(void *arg)
#define TDS_THREAD_RESULT(n) ((void*)(TDS_INTPTR)(n))

#define tds_thread_create(ret, proc, arg) \
	FreeTDS_Thread_not_compiled

#define tds_thread_create_detached(proc, arg) \
	FreeTDS_Thread_not_compiled

#define tds_thread_join(th, ret) \
	FreeTDS_Thread_not_compiled

static inline tds_thread_id tds_thread_get_current_id(void)
{
	return 0;
}

static inline int tds_thread_is_current(tds_thread_id th)
{
	return 1;
}

#endif

#  define tds_cond_init tds_raw_cond_init
#  define tds_cond_destroy tds_raw_cond_destroy
#  define tds_cond_signal tds_raw_cond_signal
#  if !ENABLE_EXTRA_CHECKS || !defined(TDS_HAVE_MUTEX)
#    define TDS_MUTEX_INITIALIZER TDS_RAW_MUTEX_INITIALIZER
#    define tds_mutex tds_raw_mutex
#    define tds_mutex_lock tds_raw_mutex_lock
#    define tds_mutex_trylock tds_raw_mutex_trylock
#    define tds_mutex_unlock tds_raw_mutex_unlock
#    define tds_mutex_check_owned(mtx) do {} while(0)
#    define tds_mutex_init tds_raw_mutex_init
#    define tds_mutex_free tds_raw_mutex_free
#    define tds_cond_wait tds_raw_cond_wait
#    define tds_cond_timedwait tds_raw_cond_timedwait
#  else
#    include <assert.h>

typedef struct tds_mutex
{
	tds_raw_mutex mtx;
	volatile int locked;
	volatile tds_thread_id locked_by;
} tds_mutex;

#   define TDS_MUTEX_INITIALIZER { TDS_RAW_MUTEX_INITIALIZER, 0 }

static inline void tds_mutex_lock(tds_mutex *mtx)
{
	assert(mtx);
	tds_raw_mutex_lock(&mtx->mtx);
	assert(!mtx->locked);
	mtx->locked = 1;
	mtx->locked_by = tds_thread_get_current_id();
}

static inline int tds_mutex_trylock(tds_mutex *mtx)
{
	int ret;
	assert(mtx);
	ret = tds_raw_mutex_trylock(&mtx->mtx);
	if (!ret) {
		assert(!mtx->locked);
		mtx->locked = 1;
		mtx->locked_by = tds_thread_get_current_id();
	}
	return ret;
}

static inline void tds_mutex_unlock(tds_mutex *mtx)
{
	assert(mtx && mtx->locked);
	mtx->locked = 0;
	tds_raw_mutex_unlock(&mtx->mtx);
}

static inline void tds_mutex_check_owned(tds_mutex *mtx)
{
	int ret;
	assert(mtx);
	ret = tds_raw_mutex_trylock(&mtx->mtx);
	assert(ret);
	assert(mtx->locked);
	assert(tds_thread_is_current(mtx->locked_by));
}

static inline int tds_mutex_init(tds_mutex *mtx)
{
	mtx->locked = 0;
	return tds_raw_mutex_init(&mtx->mtx);
}

static inline void tds_mutex_free(tds_mutex *mtx)
{
	assert(mtx && !mtx->locked);
	tds_raw_mutex_free(&mtx->mtx);
}

static inline int tds_cond_wait(tds_condition *cond, tds_mutex *mtx)
{
	int ret;
	assert(mtx && mtx->locked);
	mtx->locked = 0;
	ret = tds_raw_cond_wait(cond, &mtx->mtx);
	mtx->locked = 1;
	mtx->locked_by = tds_thread_get_current_id();
	return ret;
}

static inline int tds_cond_timedwait(tds_condition *cond, tds_mutex *mtx, int timeout_sec)
{
	int ret;
	assert(mtx && mtx->locked);
	mtx->locked = 0;
	ret = tds_raw_cond_timedwait(cond, &mtx->mtx, timeout_sec);
	mtx->locked = 1;
	mtx->locked_by = tds_thread_get_current_id();
	return ret;
}

#  endif

#endif
