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

#ifndef _tdsstring_h_
#define _tdsstring_h_

static char rcsid_tdsstring_h[] = "$Id: tdsstring.h,v 1.10 2003-12-09 10:19:16 freddy77 Exp $";
static void *no_unused_tdsstring_h_warn[] = { rcsid_tdsstring_h, no_unused_tdsstring_h_warn };

extern char tds_str_empty[];

/* TODO do some function and use inline if available */

/** \addtogroup dstring
 *  \@{ 
 */

#if ENABLE_EXTRA_CHECKS
void tds_dstr_init(DSTR * s);
int tds_dstr_isempty(DSTR * s);
char *tds_dstr_cstr(DSTR * s);
size_t tds_dstr_len(DSTR * s);
#else
/** init a string with empty */
#define tds_dstr_init(s) \
	{ *s = (DSTR) &tds_str_empty[0]; }

/** test if string is empty */
#define tds_dstr_isempty(s) \
	(**((char**)s) == '\0')
#define tds_dstr_cstr(s) \
	(*(char**)s)
#define tds_dstr_len(s) \
	strlen(*(char**)s)
#endif

void tds_dstr_zero(DSTR * s);
void tds_dstr_free(DSTR * s);

DSTR tds_dstr_copy(DSTR * s, const char *src);
DSTR tds_dstr_copyn(DSTR * s, const char *src, unsigned int length);
DSTR tds_dstr_set(DSTR * s, char *src);

/** \@} */

#endif /* _tdsstring_h_ */
