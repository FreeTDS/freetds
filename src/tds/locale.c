/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2005  Frediano Ziglio
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "tds.h"
#include "tds_configs.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: locale.c,v 1.26 2006-12-26 14:56:21 freddy77 Exp $");


static void tds_parse_locale(const char *option, const char *value, void *param);

/**
 * Get locale information. 
 * @return allocated structure with all information or NULL if error
 */
TDSLOCALE *
tds_get_locale(void)
{
	TDSLOCALE *locale;
	char *s;
	FILE *in;

	/* allocate a new structure with hard coded and build-time defaults */
	locale = tds_alloc_locale();
	if (!locale)
		return NULL;

	tdsdump_log(TDS_DBG_INFO1, "Attempting to read locales.conf file\n");

	in = fopen(FREETDS_LOCALECONFFILE, "r");
	if (in) {
		tds_read_conf_section(in, "default", tds_parse_locale, locale);

		s = getenv("LANG");
		if (s && s[0]) {
			int found;
			char buf[128];
			const char *strip = "@._";
			const char *charset = NULL;

			/* do not change environment !!! */
			tds_strlcpy(buf, s, sizeof(buf));

			/* search full name */
			rewind(in);
			found = tds_read_conf_section(in, buf, tds_parse_locale, locale);

			/*
			 * Here we try to strip some part of language in order to
			 * catch similar language
			 * LANG is composed by 
			 *   language[_sublanguage][.charset][@modified]
			 * ie it_IT@euro or it_IT.UTF-8 so we strip in order
			 * modifier, charset and sublanguage
			 * ie it_IT@euro -> it_IT -> it
			 */
			for (;!found && *strip; ++strip) {
				s = strrchr(buf, *strip);
				if (!s)
					continue;
				*s = 0;
				if (*strip == '.')
					charset = s+1;
				rewind(in);
				found = tds_read_conf_section(in, buf, tds_parse_locale, locale);
			}

			/* charset specified in LANG ?? */
			if (charset) {
				if (locale->server_charset)
					free(locale->server_charset);
				locale->server_charset = strdup(charset);
			}
		}

		fclose(in);
	}
	return locale;
}

static void
tds_parse_locale(const char *option, const char *value, void *param)
{
	TDSLOCALE *locale = (TDSLOCALE *) param;

	if (!strcmp(option, TDS_STR_CHARSET)) {
		if (locale->server_charset)
			free(locale->server_charset);
		locale->server_charset = strdup(value);
	} else if (!strcmp(option, TDS_STR_LANGUAGE)) {
		if (locale->language)
			free(locale->language);
		locale->language = strdup(value);
	} else if (!strcmp(option, TDS_STR_DATEFMT)) {
		if (locale->date_fmt)
			free(locale->date_fmt);
		locale->date_fmt = strdup(value);
	}
}
