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
#include "tdsutil.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char  software_version[]   = "$Id: locale.c,v 1.11 2002-10-13 23:28:12 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


static int tds_read_locale_section(FILE *in, char *section, TDSLOCINFO *config);

TDSLOCINFO *tds_get_locale(void)
{
TDSLOCINFO *locale;
unsigned char *s;
int i;
FILE *in;

	/* allocate a new structure with hard coded and build-time defaults */
	locale = tds_alloc_locale();
	tdsdump_log(TDS_DBG_INFO1, "%L Attempting to read locales.conf file\n");

	in = fopen(FREETDS_LOCALECONFFILE, "r");
	if (in) {
		tds_read_locale_section(in, "default", locale);

		s = getenv("LANG");
		if (s && strlen(s)) {
			rewind(in);
			for (i=0;i<strlen(s);i++) s[i]=tolower(s[i]);
			tds_read_locale_section(in, s, locale);
		}

		fclose(in);
	}
	return locale;
}

static int tds_read_locale_section(FILE *in, char *section, TDSLOCINFO *locale)
{
char line[256], option[256], value[256];
unsigned char *s;
unsigned char p;
int i;
int insection = 0;
int found = 0;

	while (fgets(line, 256, in)) {
		s = line;

		/* skip leading whitespace */
		while (*s && isspace(*s)) s++;

		/* skip it if it's a comment line */
		if (*s==';' || *s=='#') continue;

		/* read up to the = ignoring duplicate spaces */
		p = 0; i = 0;
		while (*s && *s!='=') {
			if (!isspace(*s) && isspace(p)) 
				option[i++]=' ';
			if (!isspace(*s)) 
				option[i++]=tolower(*s);
			p = *s;
			s++;
		}
		option[i]='\0';

		/* skip the = */
		if(*s) s++;

		/* skip leading whitespace */
		while (*s && isspace(*s)) s++;

		/* read up to a # ; or null ignoring duplicate spaces */
		p = 0; i = 0;
		while (*s && *s!=';' && *s!='#') {
			if (!isspace(*s) && isspace(p)) 
				value[i++]=' ';
			if (!isspace(*s)) 
				value[i++]=*s;
			p = *s;
			s++;
		}
		value[i]='\0';
		
		if (!strlen(option)) 
			continue;

		if (option[0]=='[') {
			s = &option[1];
			while (*s) {
				if (*s==']') *s='\0';
				s++;
			}
			if (!strcmp(section, &option[1])) {
				tdsdump_log(TDS_DBG_INFO1, "%L Found matching section\n");
				insection=1;
				found=1;
			} else {
				insection=0;
			}
		} else if (insection) {
			if (!strcmp(option,TDS_STR_CHARSET)) {
				if (locale->char_set) free(locale->char_set);
				locale->char_set = strdup(value);
			} else if (!strcmp(option,TDS_STR_LANGUAGE)) {
				if (locale->language) free(locale->language);
				locale->language = strdup(value);
			} else if (!strcmp(option,TDS_STR_DATEFMT)) {
				if (locale->date_fmt) free(locale->date_fmt);
				locale->date_fmt = strdup(value);
			}
		}

	}
	return found;
}

