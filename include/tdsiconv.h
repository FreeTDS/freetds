/* Generated from tds_configs.h.in on Sun Aug 11 12:35:14 EDT 2002 */
/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2002  Brian Bruns
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

#ifndef _tds_iconv_h_
#define _tds_iconv_h_

static char rcsid_tds_iconv_h[] = "$Id: tdsiconv.h,v 1.4 2003-03-28 12:39:06 freddy77 Exp $";
static void *no_unused_tds_iconv_h_warn[] = { rcsid_tds_iconv_h, no_unused_tds_iconv_h_warn };

#if HAVE_ICONV
#include <iconv.h>
#endif

#ifdef __cplusplus
extern "C"
{
#if 0
}
#endif
#endif

typedef struct tdsiconvinfo
{
	int use_iconv;
#if HAVE_ICONV
	char client_charset[64];
	iconv_t cdto_ucs2;   /* conversion from client charset to UCS2LE MSSQLServer */
	iconv_t cdfrom_ucs2; /* conversion from UCS2LE MSSQLServer to client charset */
	iconv_t cdto_srv;    /* conversion from client charset to SQL Server ASCII charset */
	iconv_t cdfrom_srv;  /* conversion from SQL Server ASCII charset  to client charset */
#endif
}
TDSICONVINFO;

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#endif /* _tds_iconv_h_ */
