#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>

int main(int argc, char **argv)
{
  int len;
#if defined(CHECK_ASPRINTF)
  char *p = NULL; len = asprintf(&p, "%d", 123);
#elif defined(CHECK_VASPRINTF)
  va_list va; char *p = NULL; len = vasprintf(&p, "%d", va);
#elif defined(CHECK_SNPRINTF)
  char buf[128]; len = snprintf(buf, 128, "%d", 123);
#elif defined(CHECK__SNPRINTF)
  char buf[128]; len = _snprintf(buf, 128, "%d", 123);
#elif defined(CHECK_VSNPRINTF)
  va_list va; char buf[128]; len = vsnprintf(buf, 128, "%d", va);
#elif defined(CHECK__VSNPRINTF)
  va_list va; char buf[128]; len = _vsnprintf(buf, 128, "%d", va);
#elif defined(CHECK__VSCPRINTF)
  va_list va; len = _vscprintf("%d", va);
#else
#error "Check function not specified correctly"
#endif
  return !!len;
}
