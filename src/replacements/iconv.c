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

#if ! HAVE_ICONV

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include <assert.h>
#include <ctype.h>

#include "tds.h"
#include "tdsiconv.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: iconv.c,v 1.16 2008-01-20 14:23:59 freddy77 Exp $");

/**
 * \addtogroup conv
 * @{ 
 */

/** 
 * Inputs are FreeTDS canonical names, no other. No alias list is consulted.  
 */
iconv_t 
tds_sys_iconv_open (const char* tocode, const char* fromcode)
{
	int i;
	unsigned int fromto;
	const char *enc_name;
	unsigned char encodings[2] = { 0xFF, 0xFF };

	static char first_time = 1;

	if (first_time) {
		first_time = 0;
		tdsdump_log(TDS_DBG_INFO1, "Using trivial iconv\n");
	}
	
	/* match both inputs to our canonical names */
	enc_name = fromcode;
	for (i=0; i < 2; ++i) {

		if (strcmp(enc_name, "ISO-8859-1") == 0) {
			encodings[i] = 0;
		} else if (strcmp(enc_name, "US-ASCII") == 0) {
			encodings[i] = 1;
		} else if (strcmp(enc_name, "UCS-2LE") == 0) {
			encodings[i] = 2;
		} else if (strcmp(enc_name, "UTF-8") == 0) {
			encodings[i] = 3;
		}

		enc_name = tocode;
	}
	
	fromto = (encodings[0] << 4) | (encodings[1] & 0x0F);

	/* like to like */
	if (encodings[0] == encodings[1]) {
		fromto = Like_to_Like;
	}
	
	switch (fromto) {
	case Like_to_Like:
	case Latin1_ASCII:
	case ASCII_Latin1:
	case Latin1_UCS2LE:
	case UCS2LE_Latin1:
	case ASCII_UCS2LE:
	case UCS2LE_ASCII:
	case Latin1_UTF8:
	case UTF8_Latin1:
	case ASCII_UTF8:
	case UTF8_ASCII:
	case UCS2LE_UTF8:
	case UTF8_UCS2LE:
		return (iconv_t) (TDS_INTPTR) fromto;
		break;
	default:
		break;
	}
	errno = EINVAL;
	return (iconv_t)(-1);
} 

int 
tds_sys_iconv_close (iconv_t cd)
{
	return 0;
}

size_t 
tds_sys_iconv (iconv_t cd, const char* * inbuf, size_t *inbytesleft, char* * outbuf, size_t *outbytesleft)
{
	size_t copybytes;
	const unsigned char *p;
	unsigned char ascii_mask = 0;
	unsigned int n;
	const unsigned char *ib;
	unsigned char *ob;
	size_t il, ol;
	int local_errno;

#undef CD
#define CD ((int) (TDS_INTPTR) cd)

	/* iconv defines valid semantics for NULL inputs, but we don't support them. */
	if (!inbuf || !*inbuf || !inbytesleft || !outbuf || !*outbuf || !outbytesleft)
		return 0;
	
	/* 
	 * some optimizations
	 * - do not use errno directly only assign a time
	 *   (some platform define errno as a complex macro)
	 * - some processors have few registers, deference and copy input variable
	 *   (this make also compiler optimize more due to removed aliasing)
	 *   also we use unsigned to remove required unsigned casts
	 */
	local_errno = 0;
	il = *inbytesleft;
	ol = *outbytesleft;
	ib = (const unsigned char*) *inbuf;
	ob = (unsigned char*) *outbuf;

	copybytes = (il < ol)? il : ol;

	switch (CD) {
	case ASCII_UTF8:
	case UTF8_ASCII:
	case Latin1_ASCII:
		for (p = ib; p < ib + il; ++p) {
			if (*p & 0x80) {
				local_errno = EILSEQ;
				copybytes = p - ib;
				break;
			}
		}
		/* fall through */
	case ASCII_Latin1:
	case Like_to_Like:
		memcpy(ob, ib, copybytes);
		ob += copybytes;
		ol -= copybytes;
		ib += copybytes;
		il -= copybytes;
		break;
	case ASCII_UCS2LE:
	case Latin1_UCS2LE:
		if (CD == ASCII_UCS2LE)
			ascii_mask = 0x80;
		while (il > 0 && ol > 1) {
			if ((ib[0] & ascii_mask) != 0) {
				local_errno = EILSEQ;
				break;
			}
			*ob++ = *ib++;
			*ob++ = '\0';
			ol -= 2;
			--il;
		}
		break;
	case UCS2LE_ASCII:
	case UCS2LE_Latin1:
		if (CD == UCS2LE_ASCII)
			ascii_mask = 0x80;
		while (il > 1 && ol > 0) {
			if ( ib[1] || (ib[0] & ascii_mask) != 0) {
				local_errno = EILSEQ;
				break;
			}
			*ob++ = *ib;
			--ol;

			ib += 2;
			il -= 2;
		}
		/* input should be an even number of bytes */
		if (!local_errno && il == 1 && ol > 0)
			local_errno = EINVAL;
		break;
	case UTF8_Latin1:
		while (il > 0 && ol > 0) {
			/* silly case, ASCII */
			if ( (ib[0] & 0x80) == 0) {
				*ob++ = *ib++;
				--il;
				--ol;
				continue;
			}

			if (il == 1) {
				local_errno = EINVAL;
				break;
			}

			if ( ib[0] > 0xC3 || ib[0] < 0xC0 || (ib[1] & 0xC0) != 0x80) {
				local_errno = EILSEQ;
				break;
			}

			*ob++ = (*ib) << 6 | (ib[1] & 0x3F);
			--ol;
			ib += 2;
			il -= 2;
		}
		break;
	case Latin1_UTF8:
		while (il > 0 && ol > 0) {
			/* silly case, ASCII */
			if ( (ib[0] & 0x80) == 0) {
				*ob++ = *ib++;
				--il;
				--ol;
				continue;
			}

			if (ol == 1)
				break;
			*ob++ = 0xC0 | (ib[0] >> 6);
			*ob++ = 0x80 | (ib[0] & 0x3F);
			ol -= 2;
			++ib;
			--il;
		}
		break;
	case UTF8_UCS2LE:
		while (il > 0 && ol > 1) {
			/* silly case, ASCII */
			if ( (ib[0] & 0x80) == 0) {
				*ob++ = *ib++;
				*ob++ = 0;
				il -= 1;
				ol -= 2;
				continue;
			}

			if (il == 1) {
				local_errno = EINVAL;
				break;
			}

			if ( (ib[0] & 0xE0) == 0xC0) {
				if ( (ib[1] & 0xC0) != 0x80) {
					local_errno = EILSEQ;
					break;
				}

				*ob++ = ((ib[0] & 0x3) << 6) | (ib[1] & 0x3F);
				*ob++ = (ib[0] & 0x1F) >> 2;
				ol -= 2;
				ib += 2;
				il -= 2;
				continue;
			}

			if (il == 2) {
				local_errno = EINVAL;
				break;
			}

			if ( (ib[0] & 0xF0) == 0xE0) {
				if ( (ib[1] & 0xC0) != 0x80 || (ib[2] & 0xC0) != 0x80) {
					local_errno = EILSEQ;
					break;
				}

				*ob++ = ((ib[1] & 0x3) << 6) | (ib[2] & 0x3F);
				*ob++ = (ib[0] & 0xF) << 4 | ((ib[1] & 0x3F) >> 2);
				ol -= 2;
				ib += 3;
				il -= 3;
				continue;
			}

			local_errno = EILSEQ;
			break;
		}
		break;
	case UCS2LE_UTF8:
		while (il > 1 && ol > 0) {
			n = ((unsigned int)ib[1]) << 8 | ib[0];
			/* ASCII */
			if ( n < 0x80 ) {
				*ob++ = n;
				ol -= 1;
				ib += 2;
				il -= 2;
				continue;
			}

			if (ol == 1)
				break;

			if ( n < 0x800 ) {
				*ob++ = 0xC0 | (n >> 6);
				*ob++ = 0x80 | (n & 0x3F);
				ol -= 2;
				ib += 2;
				il -= 2;
				continue;
			}

			if (ol == 2)
				break;

			*ob++ = 0xE0 | (n >> 12);
			*ob++ = 0x80 | ((n >> 6) & 0x3F);
			*ob++ = 0x80 | (n & 0x3F);
			ol -= 3;
			ib += 2;
			il -= 2;
		}
		break;
	default:
		local_errno = EINVAL;
		break;
	}

	/* back to source */
	*inbytesleft = il;
	*outbytesleft = ol;
	*inbuf = (const char*) ib;
	*outbuf = (char*) ob;

	if (il && !local_errno)
		local_errno = E2BIG;
	
	if (local_errno) {
		errno = local_errno;
		return (size_t)(-1);
	}
	
	return 0;
}


/** @} */

#endif
