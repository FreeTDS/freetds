/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2003 James K. Lowden, based on original work by Brian Bruns
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

#if ! HAVE_ICONV

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include <assert.h>

#include "tds.h"
#include "tdsiconv.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "../tds/encodings.h"

static char software_version[] = "$Id: iconv.c,v 1.2 2003-07-01 05:33:14 jklowden Exp $";
static void *no_unused_var_warn[] = {
	software_version,
	no_unused_var_warn
};

/**
 * \defgroup conv Charset conversion
 * Convert between ASCII and Unicode
 */

/**
 * \addtogroup conv
 * \@{ 
 */

/** 
 * Inputs are FreeTDS canonical names, no other. No alias list is consulted.  
 */
iconv_t 
iconv_open (const char* tocode, const char* fromcode)
{
	typedef struct _fromto { char *name; unsigned char pos; } FROMTO;
	int i, ipos;
	unsigned short fromto;
	static char first_time = 1; 
	FROMTO encodings[2] = { {NULL, 0xFF}, {NULL, 0xFF} };
	encodings[0].name = (char*)fromcode;
	encodings[1].name = (char*)tocode;
	
	
	if (first_time) {
		first_time = 0;
		tdsdump_log(TDS_DBG_INFO1, "Using trivial iconv from \n\t%s\n", __FILE__);
	}
	
	/* match both inputs to our canonical names */
	for (i=0; i < sizeof(encodings)/sizeof(FROMTO); i++) {
		for (ipos=0; canonic_charsets[ipos].min_bytes_per_char > 0; ipos++) {
			if (0 == strncmp(encodings[i].name, canonic_charsets[ipos].name, strlen(canonic_charsets[ipos].name))) {
				encodings[i].pos = ipos;
			}
		}
	}
	
	/* like to like */
	if (encodings[0].pos == encodings[1].pos) {
		fromto = Like_to_Like;
	}
	
	fromto = (encodings[0].pos << 4) | (encodings[1].pos & 0x0F);
	
	switch (fromto) {
	case Like_to_Like:
	case Latin1_ASCII:
	case ASCII_Latin1:
	case Latin1_UCS2LE:
	case UCS2LE_Latin1:
	case ASCII_UCS2LE:
	case UCS2LE_ASCII:
		return (iconv_t)(unsigned int)fromto;
		break;
	default:
		errno = EINVAL;
		return (iconv_t)(-1);
		break;
	}
	return (iconv_t)(-1);
} 

int 
iconv_close (iconv_t cd)
{
	return 0;
}

size_t 
iconv (iconv_t cd, const char* * inbuf, size_t *inbytesleft, char* * outbuf, size_t *outbytesleft)
{
	enum {FALSE, TRUE};
	int copybytes;
	const char *p;
	int finvalid = FALSE;
	
	/* iconv defines valid semantics for NULL inputs, but we don't support them. */
	assert(inbuf && inbytesleft && outbuf && outbytesleft);
	
	copybytes = (*inbytesleft < *outbytesleft)? *inbytesleft : *outbytesleft;

	switch ((int)cd) {
	case Latin1_ASCII:
		for (p = *inbuf; p < *inbuf + *inbytesleft; p++) {
			if (!isascii(*p)) {
				errno = EINVAL;
				copybytes = p - *inbuf;
				finvalid = TRUE;
				break;
			}
		}
		/* fall through */
	case ASCII_Latin1:
	case Like_to_Like:
		memcpy(*outbuf, *inbuf, copybytes);
		*inbuf += copybytes;
		*outbuf += copybytes;
		*inbytesleft -= copybytes;
		*outbytesleft -= copybytes;
		if (finvalid) {
			return (size_t)(-1);
		}		
		break;
	case ASCII_UCS2LE:
	case Latin1_UCS2LE:
		while (*inbytesleft > 0 && *outbytesleft > 1) {
			if ((int)cd == ASCII_UCS2LE && !isascii(**inbuf)) {
				errno = EINVAL;
				return (size_t)(-1);
			}
			*(*outbuf)++ = '\0';
			*(*outbuf)++ = *(*inbuf)++;
			*inbytesleft--;
			*outbytesleft -= 2;
		}
		break;
	case UCS2LE_ASCII:
	case UCS2LE_Latin1:
		/* input should be an even number of bytes */
		if (*inbytesleft & 1) {
			errno = EINVAL;
			return (size_t)(-1);
		}
		while (*inbytesleft > 1 && *outbytesleft > 0) {
			*inbytesleft--;
			if (*(*inbuf)++ != '\0') {
				errno = EILSEQ;
				return (size_t)(-1);
			}
			if ((int)cd == UCS2LE_ASCII && !isascii(**inbuf)) {
				errno = EINVAL;
				return (size_t)(-1);
			}
			*(*outbuf)++ = *(*inbuf)++;
			*inbytesleft--;
			*outbytesleft--;
		}
		break;
	default:
		errno = EINVAL;
		return (size_t)(-1);
		break;
	}
	
	if (*inbytesleft) {
		errno = E2BIG;
		return (size_t)(-1);
	}
	
	return 0;
}


/** \@} */

#endif
