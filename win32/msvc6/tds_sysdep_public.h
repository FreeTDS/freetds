/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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

#ifndef _tds_sysdep_public_h_
#define _tds_sysdep_public_h_

static char rcsid_tds_sysdep_public_h[] = "$Id: tds_sysdep_public.h,v 1.2 2003-02-06 09:27:21 freddy77 Exp $";
static void *no_unused_tds_sysdep_public_h_warn[] = { rcsid_tds_sysdep_public_h, no_unused_tds_sysdep_public_h_warn };

#ifdef __cplusplus
extern "C"
{
#endif

#include <windows.h>
#define tds_sysdep_int16_type short	/* 16-bit int */
#define tds_sysdep_int32_type int	/* 32-bit int */
#define tds_sysdep_int64_type __int64	/* 64-bit int */
#define tds_sysdep_real32_type float	/* 32-bit real */
#define tds_sysdep_real64_type double	/* 64-bit real */

#ifdef __cplusplus
}
#endif

#endif				/* _tds_sysdep_public_h_ */
