/*
 * vasprintf(3), asprintf(3)
 * 20020809 entropy@tappedin.com
 * public domain.  no warranty.  use at your own risk.  have a nice day.
 */

#if !HAVE_VASPRINTF

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

static char  software_version[]   = "$Id: asprintf.c,v 1.7 2002-09-17 01:40:12 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

#define CHUNKSIZE 512

int
vasprintf(char **ret, const char *fmt, va_list ap)
{
#ifdef HAVE_VSNPRINTF
  int chunks;
  size_t buflen;
  int fit = 0;
  char *buf;
  int len;

  chunks = ((strlen(fmt) + 1) / CHUNKSIZE) + 1;
  buflen = chunks * CHUNKSIZE;
  while (!fit) {
    if ((buf = malloc(buflen)) == NULL) {
      *ret = NULL;
      return -1;
    }
    len = vsnprintf(buf, buflen, fmt, ap);
    if (len >= buflen) {
      free(buf);
      buflen = (++chunks) * CHUNKSIZE;
      if (len >= buflen) {
        buflen = len + 1;
      }
    } else {
      fit = 1;
    }
  }
  if (len < 0) {
    free(buf);
    *ret = NULL;
    return len;
  }
  *ret = buf;
  return len;
#else
  FILE *fp;
  int len;
  char *buf;

  *ret = NULL;
  if ((fp = fopen("/dev/null", "w")) == NULL)
    return -1;
  len = vfprintf(fp, fmt, ap);
  if (fclose(fp) != 0)
    return -1;
  if (len < 0)
    return len;
  if ((buf = malloc(len + 1)) == NULL)
    return -1;
  if (vsprintf(buf, fmt, ap) != len)
    return -1;
  *ret = buf;
  return len;
#endif
}

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
#endif /* HAVE_VASPRINTF */
