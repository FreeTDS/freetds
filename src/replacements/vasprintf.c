/*
 * vasprintf(3)
 * 20020809 entropy@tappedin.com
 * public domain.  no warranty.  use at your own risk.  have a nice day.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <stdarg.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_PATHS_H
#include <paths.h>
#endif /* HAVE_PATHS_H */

#include "replacements.h"

static char  software_version[]   = "$Id: vasprintf.c,v 1.9 2002-10-15 12:41:20 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

#define CHUNKSIZE 512
int
vasprintf(char **ret, const char *fmt, va_list ap)
{
#if HAVE_VSNPRINTF
  int chunks;
  size_t buflen;
  char *buf;
  int len;

  chunks = ((strlen(fmt) + 1) / CHUNKSIZE) + 1;
  buflen = chunks * CHUNKSIZE;
  for (;;) {
    if ((buf = malloc(buflen)) == NULL) {
      *ret = NULL;
      return -1;
    }
    len = vsnprintf(buf, buflen, fmt, ap);
    if (len >=0 && len < buflen) {
      break;
    }
    free(buf);
    buflen = (++chunks) * CHUNKSIZE;
    if (len >= buflen) {
      buflen = len + 1;
    }
  }
  *ret = buf;
  return len;
#else /* HAVE_VSNPRINTF */
#ifdef _REENTRANT
  FILE *fp;
#else /* !_REENTRANT */
  static FILE *fp = NULL;
#endif /* !_REENTRANT */
  int len;
  char *buf;

  *ret = NULL;

#ifdef _REENTRANT
  if ((fp = fopen(_PATH_DEVNULL, "w")) == NULL)
    return -1;
#else /* !_REENTRANT */
  if ((fp == NULL) && ((fp = fopen(_PATH_DEVNULL, "w")) == NULL))
    return -1;
#endif /* !_REENTRANT */

  len = vfprintf(fp, fmt, ap);

#ifdef _REENTRANT
  if (fclose(fp) != 0)
    return -1;
#endif /* _REENTRANT */

  if (len < 0)
    return len;
  if ((buf = malloc(len + 1)) == NULL)
    return -1;
  if (vsprintf(buf, fmt, ap) != len)
    return -1;
  *ret = buf;
  return len;
#endif /* HAVE_VSNPRINTF */
}
