/*
 * asprintf(3)
 * 20020809 entropy@tappedin.com
 * public domain.  no warranty.  use at your own risk.  have a nice day.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdarg.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "replacements.h"

static char software_version[] = "$Id: asprintf.c,v 1.5 2003-02-18 09:54:58 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };


int
asprintf(char **ret, const char *fmt, ...)
{
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vasprintf(ret, fmt, ap);
	va_end(ap);
	return len;
}
