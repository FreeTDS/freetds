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
#include "tdsiconv.h"
#if HAVE_ICONV
#include <iconv.h>
#endif
#ifdef DMALLOC
#include <dmalloc.h>
#endif

void tds_iconv_open(TDSSOCKET *tds, char *charset)
{
TDSICONVINFO *iconv_info;

	iconv_info = (TDSICONVINFO *) tds->iconv_info;

#if HAVE_ICONV
	iconv_info->cdto = iconv_open("UCS-2LE",charset);
	if (iconv_info->cdto == (iconv_t)-1) {
		iconv_info->use_iconv = 0;
		return;
	}
	iconv_info->cdfrom = iconv_open(charset, "UCS-2LE");
	if (iconv_info->cdfrom == (iconv_t)-1) {
		iconv_info->use_iconv = 0;
		return;
	}
	iconv_info->use_iconv = 1; 
	/* temporarily disable */
	/* iconv_info->use_iconv = 0; */
#else 
	iconv_info->use_iconv = 0;
#endif
}
void tds_iconv_close(TDSSOCKET *tds)
{
TDSICONVINFO *iconv_info;

	iconv_info = (TDSICONVINFO *) tds->iconv_info;

#if HAVE_ICONV
	if (iconv_info->cdto != (iconv_t)-1) {
		iconv_close(iconv_info->cdto);
	}
	if (iconv_info->cdfrom != (iconv_t)-1) {
		iconv_close(iconv_info->cdfrom);
	}
#endif 
}

/**
 * tds7_unicode2ascii()
 * Note: The dest buf must be large enough to handle 'len' + 1 bytes.
 * in_string have not to be terminated and len characters (2 byte) long
 */
char *tds7_unicode2ascii(TDSSOCKET *tds, const char *in_string, char *out_string, int len)
{
int i;
#if HAVE_ICONV
TDSICONVINFO *iconv_info;
const char *in_ptr;
char *out_ptr;
size_t out_bytes, in_bytes;
char quest_mark[] = "?\0"; /* best to live no-const */
char *pquest_mark; 
size_t lquest_mark;
#endif

	if (!in_string) return NULL;

#if HAVE_ICONV
	iconv_info = tds->iconv_info;
	if (iconv_info->use_iconv) {
     	out_bytes = len;
     	in_bytes = len * 2;
     	in_ptr = (char *)in_string;
     	out_ptr = out_string;
     	while (iconv(iconv_info->cdfrom, &in_ptr, &in_bytes, &out_ptr, &out_bytes) == (size_t)-1) {
		/* iconv call can reset errno */
		i = errno;
		/* reset iconv state */
		iconv(iconv_info->cdfrom, NULL, NULL, NULL, NULL);
		if (i != EILSEQ) break;

		/* skip one UCS-2 sequnce */
		in_ptr += 2;
		in_bytes -= 2;

		/* replace invalid with '?' */
		pquest_mark = quest_mark;
		lquest_mark = 2;
		iconv(iconv_info->cdfrom, &pquest_mark, &lquest_mark, &out_ptr, &out_bytes);
		if (out_bytes == 0) break;
	}
	/* FIXME best method ?? there is no way to return 
	 * less or more than len characters */
	/* something went wrong fill remaining with zeroes 
	 * avoiding returning garbage data */
	if (out_bytes) memset(out_ptr,0,out_bytes);
	out_string[len] = '\0';

     	return out_string;
	}
#endif

	/* no iconv, strip high order byte if zero or replace with '?' 
	 * this is the same of converting to ISO8859-1 charset using iconv */
	/* FIXME update docs */
	for (i=0;i<len;++i) {
		out_string[i] = 
			in_string[i*2+1] ? '?' : in_string[i*2];
	}
	out_string[i]='\0';
	return out_string;
}

/**
 * tds7_ascii2unicode()
 * Convert a string to Unicode
 * Note: output string is not terminated
 * @param in_string string to translate, null terminated
 * @param out_string buffer to store translated string
 * @param maxlen length of out_string buffer in bytes
 */
unsigned char *
tds7_ascii2unicode(TDSSOCKET *tds, const char *in_string, char *out_string, int maxlen)
{
register int out_pos = 0;
register int i; 
size_t string_length;
#if HAVE_ICONV
TDSICONVINFO *iconv_info;
const char *in_ptr;
char *out_ptr;
size_t out_bytes, in_bytes;
#endif

	if (!in_string) return NULL;
	string_length = strlen(in_string);

#if HAVE_ICONV
	iconv_info = tds->iconv_info;
	if (iconv_info->use_iconv) {
     	out_bytes = maxlen;
     	in_bytes = string_length;
     	in_ptr = (char *)in_string;
     	out_ptr = out_string;
     	iconv(iconv_info->cdto, &in_ptr, &in_bytes, &out_ptr, &out_bytes);

     	return out_string;
	}
#endif

	/* no iconv, add null high order byte to convert 7bit ascii to unicode */
	if (string_length*2 > maxlen)
		string_length = maxlen >> 1;

	for (i=0;i<string_length;i++) {
		out_string[out_pos++]=in_string[i];	
		out_string[out_pos++]='\0';
	}

	return out_string;
}

