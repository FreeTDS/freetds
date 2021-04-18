#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
#if defined(CHECK_ASPRINTF)
  char *p = NULL; int len = asprintf(&p, "%d", 123);
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
#elif defined(CHECK_THREAD_LOCAL)
  static thread_local int i;
#elif defined(CHECK__THREAD_LOCAL)
  static _Thread_local int i;
#elif defined(CHECK___THREAD)
  static __thread int i;
#elif defined(CHECK__DECLSPEC_THREAD)
  static __declspec(thread) int i;
#else
#error "Check function not specified correctly"
#endif
  return 0;
}
