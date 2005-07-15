/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2005 Ziglio Frediano
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

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds_sysdep_private.h"
#include "replacements.h"

#if ! HAVE_BASENAME

TDS_RCSID(var, "$Id: basename.c,v 1.2 2005-07-15 11:52:18 freddy77 Exp $");

char *tds_basename(char *path)
{
	char *p;

	if (path == NULL)
		return path;

	for (p = path + strlen(path); --p > path && *p == '/';)
		*p = '\0';

	p = strrchr(path, '/');

	return p ? p : path;
}
#endif

