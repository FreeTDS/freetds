/*
 * strtok_r(3)
 * 20020927 entropy@tappedin.com
 * public domain.  no warranty.  use at your own risk.  have a nice day.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>

char *
strtok_r(char *str, const char *sep, char **lasts)
{
  char *p;

  if (str == NULL) {
    str = *lasts;
  }
  if (str == NULL) {
    return NULL;
  }
  str += strspn(str, sep);
  if ((p = strpbrk(str, sep)) != NULL) {
    *lasts = p + 1;
    *p = '\0';
  } else {
    *lasts = NULL;
  }
  return str;
}
