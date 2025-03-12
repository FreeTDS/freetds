/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2024  Frediano Ziglio
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

#include <config.h>

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/macros.h>
#include <freetds/utils.h>

/**
 * Copy a string of length len to a new allocated buffer
 * This function does not read more than len bytes.
 * Please note that some system implementations of strndup
 * do not assure they don't read past len bytes as they
 * use still strlen to check length to copy limiting
 * after strlen to size passed.
 * String returned is NUL terminated.
 *
 * \param s   string to copy from
 * \param len length to copy
 *
 * \returns string copied or NULL if errors
 */
char *
tds_strndup(const void *s, TDS_INTPTR len)
{
	char *out;
	const char *end;
	TDS_UINTPTR ulen;

	if (len < 0)
		return NULL;

	ulen = (TDS_UINTPTR) len;
	end = (const char *) memchr(s, '\0', ulen);
	if (end)
		ulen = (size_t) (end - (const char *) s);

	out = tds_new(char, ulen + 1);
	if (out) {
		memcpy(out, s, ulen);
		out[ulen] = 0;
	}
	return out;
}

