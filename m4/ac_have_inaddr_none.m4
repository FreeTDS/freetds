dnl $Id: ac_have_inaddr_none.m4,v 1.1 2006-03-24 22:00:17 jklowden Exp $
AC_DEFUN([AC_HAVE_INADDR_NONE],
[AC_CACHE_CHECK([whether INADDR_NONE is defined], ac_cv_have_inaddr_none,
 [AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
],[
unsigned long foo = INADDR_NONE;
],
  ac_cv_have_inaddr_none=yes,
  ac_cv_have_inaddr_none=no)])
 if test $ac_cv_have_inaddr_none != yes; then
   AC_DEFINE(INADDR_NONE, 0xffffffff, [Define to value of INADDR_NONE if not provided by your system header files.])
 fi])




