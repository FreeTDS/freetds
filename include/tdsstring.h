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

static char rcsid_tdsstring_h[] = "$Id: tdsstring.h,v 1.6 2002-11-21 16:53:43 freddy77 Exp $";
static void *no_unused_tdsstring_h_warn[] = { rcsid_tdsstring_h, no_unused_tdsstring_h_warn };

extern char tds_str_empty[];

/* TODO do some function and use inline if available */

/** \addtogroup dstring
 *  \@{ 
 */

/** init a string with empty */
#define tds_dstr_init(s) \
	{ *(s) = tds_str_empty; }

void tds_dstr_zero(char **s);
void tds_dstr_free(char **s);

char *tds_dstr_copy(char **s, const char *src);
char *tds_dstr_copyn(char **s, const char *src, unsigned int length);
char *tds_dstr_set(char **s, char *src);

/** test if string is empty */
#define tds_dstr_isempty(s) \
	(**(s) == '\0')

/** \@} */

#endif /* _tdsstring_h_ */
