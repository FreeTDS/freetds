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

static char rcsid_tds_iconv_h[] = "$Id: tdsiconv.h,v 1.10 2003-04-13 14:06:38 jklowden Exp $";
static void *no_unused_tds_iconv_h_warn[] = { rcsid_tds_iconv_h, no_unused_tds_iconv_h_warn };

#if HAVE_ICONV
#include <iconv.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Information relevant to libiconv.  The name is an iconv name, not 
 * the same as found in master..syslanguages.  
 * \todo Write (preferably public domain) functions to convert:
 *  	- nl_langinfo output to iconv charset name
 * 	- iconv charset name to Sybase charset name
 */

typedef enum { to_server, to_client } TDS_ICONV_DIRECTION;

typedef struct _tds_encoding 
{
	char name[64];
	unsigned char min_bytes_per_char;
	unsigned char max_bytes_per_char;
} TDS_ENCODING;

struct tdsiconvinfo
{
	TDS_ENCODING client_charset;
	TDS_ENCODING server_charset;
#if HAVE_ICONV
	iconv_t to_wire;   /* conversion from client charset to server's format */
	iconv_t from_wire; /* conversion from server's format to client charset */
#endif
};

/* we use ICONV_CONST for tds_iconv(), even if we don't have iconv() */
#ifndef ICONV_CONST
# define ICONV_CONST const
#endif

size_t tds_iconv (TDS_ICONV_DIRECTION io, const TDSICONVINFO *info, ICONV_CONST char *input, size_t * input_size, char *out_string, size_t maxlen);

#ifdef __cplusplus
}
#endif

#endif /* _tds_iconv_h_ */
