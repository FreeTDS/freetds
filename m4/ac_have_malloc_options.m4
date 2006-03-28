dnl $Id: ac_have_malloc_options.m4,v 1.2 2006-03-28 00:05:30 jklowden Exp $
AC_DEFUN([AC_HAVE_MALLOC_OPTIONS],
 [AC_CACHE_CHECK([whether malloc_options variable is present],
   ac_cv_have_malloc_options,
   [AC_LINK_IFELSE(AC_LANG_SOURCE([
#include <stdlib.h>
      ],[
extern char *malloc_options;
malloc_options = "AJR";
      ]),
     ac_cv_have_malloc_options=yes,
     ac_cv_have_malloc_options=no)])
  if test $ac_cv_have_malloc_options = yes; then
   AC_DEFINE(HAVE_MALLOC_OPTIONS, 1, [Define to 1 if your system provides the malloc_options variable.])
  fi])

