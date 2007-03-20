/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2003, 2004  James K. Lowden, based on original work by Brian Bruns
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
 * This file implements a very simple iconv.  
 * Its purpose is to allow ASCII clients to communicate with Microsoft servers
 * that encode their metadata in Unicode (UCS-2).  
 *
 * The conversion algorithm relies on the fact that UCS-2 shares codepoints
 * between 0 and 255 with ISO-8859-1.  To create UCS-2, we add a high byte
 * whose value is zero.  To create ISO-8859-1, we strip the high byte.  
 *
 * If we receive an input character whose value is greater than 255, we return an 
 * out-of-range error.  The caller (tds_iconv) should emit an error message.  
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(WIN32)

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include "tds.h"
#include "replacements.h"

TDS_RCSID(var, "$Id: gettimeofday.c,v 1.3 2007-03-20 15:25:29 freddy77 Exp $");
/*
 * Number of micro-seconds between the beginning of the Windows epoch
 * (Jan. 1, 1601) and the Unix epoch (Jan. 1, 1970).
 *
 * This assumes all Win32 compilers have 64-bit support.
 */
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS) || defined(__WATCOMC__)
# define DELTA_EPOCH_IN_USEC  11644473600000000Ui64
#else
# define DELTA_EPOCH_IN_USEC  11644473600000000ULL
#endif

int gettimeofday (struct timeval *tv, void *tz)
{
	FILETIME  ft;
	TDS_UINT8 tim;
	TDS_UINT r;

	if (!tv) {
		errno = EINVAL;
		return -1;
	}
	/*
	 * Although this function returns 10^-7 precision the real 
	 * precision is less than milliseconds on Windows XP
	 */
	GetSystemTimeAsFileTime (&ft);
	tim = ((((TDS_UINT8) ft.dwHighDateTime) << 32) | ft.dwLowDateTime) -
	      (DELTA_EPOCH_IN_USEC * 10U);
	/*
	 * here we use same division to compute quotient
	 * and remainder at the same time (gcc)
	 */
	tv->tv_sec  = (long) (tim / 10000000UL);
	r = tim % 10000000UL;
	tv->tv_usec = (long) (r / 10L);
	return 0;
}

#endif
