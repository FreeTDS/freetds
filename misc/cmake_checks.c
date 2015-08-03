#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>

int main(int argc, char **argv)
{
#if defined(CHECK_ASPRINTF)
  char *p = NULL; int len = asprintf(&p, "%d", 123)");
#elif defined(CHECK_VASPRINTF)
  va_list va; char *p = NULL; int len = vasprintf(&p, "%d", va);
#elif defined(CHECK_SNPRINTF)
  char buf[128]; int len = snprintf(buf, 128, "%d", 123);
#elif defined(CHECK__SNPRINTF)
  char buf[128]; int len = _snprintf(buf, 128, "%d", 123);
#elif defined(CHECK_VSNPRINTF)
  va_list va; char buf[128]; int len = vsnprintf(buf, 128, "%d", va);
#elif defined(CHECK__VSNPRINTF)
  va_list va; char buf[128]; int len = _vsnprintf(buf, 128, "%d", va);
#elif defined(CHECK__VSCPRINTF)
  va_list va; int len = _vscprintf("%d", va);
#else
#error "Check function not specified correctly"
#endif
  return 0;
}
