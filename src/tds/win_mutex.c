/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2010  Frediano Ziglio
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef _WIN32

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#include "tdsthread.h"

TDS_RCSID(var, "$Id: win_mutex.c,v 1.1 2010-01-27 15:32:04 freddy77 Exp $");

#include "ptw32_MCS_lock.c"

void
tds_win_mutex_lock(tds_win_mutex_t * mutex)
{
	if (!InterlockedExchangeAdd(&mutex->done, 0)) {	/* MBR fence */
		ptw32_mcs_local_node_t node;

		ptw32_mcs_lock_acquire((ptw32_mcs_lock_t *) &mutex->lock, &node);
		if (!mutex->done) {
			InitializeCriticalSection(&mutex->crit);
			mutex->done = 1;
		}
		ptw32_mcs_lock_release(&node);
	}
	EnterCriticalSection(&mutex->crit);
}

#endif
