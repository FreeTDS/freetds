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

/*
** iconv.c, handle all the conversion stuff without spreading #if HAVE_ICONV 
** all over the other code
*/

#include <config.h>
#include "tds.h"
#include "tdsutil.h"
#if HAVE_ICONV
#include <iconv.h>
#endif

void tds_iconv_open(TDSSOCKET *tds, char *charset)
{
#if HAVE_ICONV
	tds->cdto = iconv_open("UCS-2",charset);
	if (tds->cdto == (iconv_t)-1) {
		tds->use_iconv = 0;
		return;
	}
	tds->cdfrom = iconv_open(charset, "UCS-2");
	if (tds->cdfrom == (iconv_t)-1) {
		tds->use_iconv = 0;
		return;
	}
	/* tds->use_iconv = 1; */
	/* temporarily disable */
	tds->use_iconv = 0;
#else 
	tds->use_iconv = 0;
#endif
}
void tds_iconv_close(TDSSOCKET *tds)
{
#if HAVE_ICONV
	if (tds->cdto != (iconv_t)-1) {
		iconv_close(tds->cdto);
	}
	if (tds->cdfrom != (iconv_t)-1) {
		iconv_close(tds->cdfrom);
	}
#endif 
}
/*
** tds7_unicode2ascii()
** Note: The dest buf must be large enough to handle 'len' + 1 bytes.
*/
char *tds7_unicode2ascii(TDSSOCKET *tds, const char *in_string, char *out_string, int len)
{
int i;

#if HAVE_ICONV
char *in_ptr, *out_ptr;
int  out_bytes, in_bytes;

	if (tds->use_iconv) {
     	out_bytes = len + 1;
     	in_bytes = (len + 1) * 2;
     	in_ptr = (char *)in_string;
     	out_ptr = (char *)out_string;
     	iconv(tds->cdfrom, &in_ptr, &in_bytes, &out_ptr, &out_bytes);

     	return out_string;
	}
#endif

	/* no iconv, strip high order byte to produce 7bit ascii */
	for (i=0;i<len;i++) {
		out_string[i]=in_string[i*2];
	}
	out_string[i]='\0';
	return out_string;
}
/*
** tds7_ascii2unicode()
*/
char *tds7_ascii2unicode(TDSSOCKET *tds, const char *in_string, char *out_string, int maxlen)
{
register int out_pos = 0;
register int i; 
size_t string_length = strlen(in_string);

#if HAVE_ICONV
char *in_ptr, *out_ptr;
int  out_bytes, in_bytes;

	if (tds->use_iconv) {
     	out_bytes = maxlen;
     	in_bytes = strlen(in_string) + 1;
     	in_ptr = (char *)in_string;
     	out_ptr = (char *)out_string;
     	iconv(tds->cdto, &in_ptr, &in_bytes, &out_ptr, &out_bytes);

     	return out_string;
	}
#endif

	/* no iconv, add null high order byte to convert 7bit ascii to unicode */
	memset(out_string, 0, string_length*2);

	for (i=0;i<string_length;i++) {
		out_string[out_pos++]=in_string[i];	
		out_string[out_pos++]='\0';
	}

	return out_string;
}

