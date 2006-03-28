dnl $Id: ac_tds_func_which_localtime_r.m4,v 1.2 2006-03-28 00:05:30 jklowden Exp $
AC_DEFUN([AC_tds_FUNC_WHICH_LOCALTIME_R],
[AC_CACHE_CHECK(for which type of localtime_r, ac_cv_func_which_localtime_r, [
	AC_COMPILE_IFELSE(AC_LANG_SOURCE([
#include <unistd.h>
#include <time.h>
  	], 	[
struct tm mytm;
time_t t;
int y = localtime_r(&t, &mytm)->tm_year;
]),ac_cv_func_which_localtime_r=struct,
  ac_cv_func_which_localtime_r=int)
])

if test $ac_cv_func_which_localtime_r = struct; then
  AC_DEFINE(HAVE_FUNC_LOCALTIME_R_TM, 1, [Define to 1 if your localtime_r return a struct tm*.])
else
  AC_DEFINE(HAVE_FUNC_LOCALTIME_R_INT, 1, [Define to 1 if your localtime_r return a int.])
fi
])

