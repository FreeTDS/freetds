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

static char rcsid_tds_iconv_h[]=
	 "$Id: tdsiconv.h,v 1.1 2002-08-14 16:34:55 brianb Exp $";
static void *no_unused_tds_iconv_h_warn[]={rcsid_tds_iconv_h, no_unused_tds_iconv_h_warn};

#if HAVE_ICONV
#include <iconv.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdsiconvinfo {
	int use_iconv;
#if HAVE_ICONV
     iconv_t cdto;
     iconv_t cdfrom;
#endif
} TDSICONVINFO;

#ifdef __cplusplus
}
#endif 

#endif /* _tds_iconv_h_ */
