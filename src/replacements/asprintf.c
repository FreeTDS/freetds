/*
 * asprintf(3)
 * 20020809 entropy@tappedin.com
 * public domain.  no warranty.  use at your own risk.  have a nice day.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include "replacements.h"

static char  software_version[]   = "$Id: asprintf.c,v 1.3 2002-10-11 14:49:34 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


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
