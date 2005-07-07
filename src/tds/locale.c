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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "tds.h"
#include "tds_configs.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: locale.c,v 1.22 2005-07-07 13:06:45 freddy77 Exp $");


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
			rewind(in);
			tds_read_conf_section(in, s, tds_parse_locale, locale);
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
		if (locale->char_set)
			free(locale->char_set);
		locale->char_set = strdup(value);
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
