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

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/tds.h>
#include <freetds/replacements.h>

/*
 * XXX The magic use of \xFF is bletcherous, but I can't think of anything
 *     better right now.
 */
#define CHARSEP '\377'

static char *
norm_fmt(const char *fmt, ptrdiff_t fmtlen, size_t *p_tokcount)
{
	char *newfmt;
	char *cp;
	bool skip = false;
	size_t tokcount = 1;

	if (fmtlen == TDS_NULLTERM) {
		fmtlen = strlen(fmt);
	}
	if ((newfmt = tds_new(char, fmtlen + 1)) == NULL)
		return NULL;

	for (cp = newfmt; fmtlen > 0; fmtlen--, fmt++) {
		switch (*fmt) {
		case ',':
		case ' ':
			if (!skip) {
				tokcount++;
				*cp++ = CHARSEP;
				skip = true;
			}
			break;
		default:
			skip = false;
			*cp++ = *fmt;
			break;
		}
	}
	*cp = '\0';
	*p_tokcount = tokcount;
	return newfmt;
}

TDSRET
tds_vstrbuild(char *buffer, int buflen, int *resultlen, const char *text, int textlen, const char *formats, int formatlen,
	      va_list ap)
{
	char *newformat;
	char *params;
	char *token;
	static const char strsep[2] = { CHARSEP, 0 };
	const char *sep = strsep;
	char *lasts;
	size_t tokcount, i;
	int state;
	char **string_array = NULL;
	unsigned int pnum = 0;
	char *paramp = NULL;
	char *const orig_buffer = buffer;
	TDSRET rc = TDS_FAIL;

	*resultlen = 0;
	if (textlen == TDS_NULLTERM)
		textlen = (int) strlen(text);

	newformat = norm_fmt(formats, formatlen, &tokcount);
	if (newformat == NULL)
		return TDS_FAIL;

	if (vasprintf(&params, newformat, ap) < 0) {
		free(newformat);
		return TDS_FAIL;
	}
	free(newformat);
	if ((string_array = tds_new(char *, tokcount + 1)) == NULL) {
		goto out;
	}
	for (token = strtok_r(params, sep, &lasts), i = 0; token != NULL && i < tokcount; token = strtok_r(NULL, sep, &lasts)) {
		string_array[i] = token;
		i++;
	}
	tokcount = i;

#define COPYING 1
#define CALCPARAM 2
#define OUTPARAM 3

	state = COPYING;
	while ((buflen > 0) && (textlen > 0 || state == OUTPARAM)) {
		switch (state) {
		case COPYING:
			if (*text == '%') {
				state = CALCPARAM;
				pnum = 0;
			} else {
				*buffer++ = *text;
				buflen--;
			}
			text++;
			textlen--;
			break;
		case CALCPARAM:
			if (*text == '!') {
				if (pnum <= 0 || pnum > tokcount)
					goto out;
				paramp = string_array[pnum - 1];
				state = OUTPARAM;
			} else {
				int pdigit = *text - '0';

				if ((pdigit >= 0) && (pdigit <= 9)) {
					pnum *= 10;
					pnum += pdigit;
				}
			}
			text++;
			textlen--;
			break;
		case OUTPARAM:
			if (*paramp == 0) {
				state = COPYING;
			} else {
				*buffer++ = *paramp++;
				buflen--;
			}
			break;
		default:
			/* unknown state */
			goto out;
		}
	}

	rc = TDS_SUCCESS;

      out:
	*resultlen = (int) (buffer - orig_buffer);
	free(string_array);
	free(params);

	return rc;
}
