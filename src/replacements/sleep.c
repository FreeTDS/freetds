/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2015 Ziglio Frediano
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

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_POLL_H
#include <poll.h>
#endif /* HAVE_POLL_H */

#include <freetds/time.h>
#include <freetds/sysdep_private.h>
#include "replacements.h"

void
tds_sleep_s(unsigned sec)
{
#ifdef _WIN32
	Sleep(sec * 1000u);
#else
	sleep(sec);
#endif
}

void
tds_sleep_ms(unsigned ms)
{
#ifdef _WIN32
	Sleep(ms);
#elif defined(HAVE_NANOSLEEP)
	struct timespec ts = { ms / 1000u, (ms % 1000u) * 1000000lu }, rem;
	int r;

	for (;;) {
		r = nanosleep(&ts, &rem);
		if (!r || errno != EINTR)
			break;
		ts = rem;
	}
#elif defined(HAVE_USLEEP)
	usleep(ms * 1000u);
#else
	struct pollfd fd;
	poll(&fd, 0, ms);
#endif
}
