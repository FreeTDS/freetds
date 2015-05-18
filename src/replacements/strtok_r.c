/*
 * strtok_r(3)
 * 20020927 entropy@tappedin.com
 * public domain.  no warranty.  use at your own risk.  have a nice day.
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/sysdep_private.h>
#include "replacements.h"

char *
tds_strtok_r(char *str, const char *sep, char **lasts)
{
	char *p;

	if (str == NULL) {
		str = *lasts;
	}
	if (str == NULL) {
		return NULL;
	}
	str += strspn(str, sep);	/* skip any separators */
	if ((p = strpbrk(str, sep)) != NULL) {
		*lasts = p + 1;
		*p = '\0';
	} else {
		if (!*str)
			str = NULL;
		*lasts = NULL;
	}
	return str;
}

