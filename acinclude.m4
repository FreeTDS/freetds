#serial AM2

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




##
# From Bruno Haible.
##
AC_DEFUN([AM_ICONV],
[
  dnl Some systems have iconv in libc, some have it in libiconv (OSF/1 and
  dnl those with the standalone portable GNU libiconv installed).

  save_LDFLAGS="$LDFLAGS"
  LIBICONV=
  AC_ARG_WITH([libiconv-prefix],
AC_HELP_STRING([--with-libiconv-prefix=DIR], [search for libiconv in DIR/include and DIR/lib]), [
    for dir in `echo "$withval" | tr : ' '`; do
      if test -d $dir/include; then CPPFLAGS="$CPPFLAGS -I$dir/include"; fi
      if test -d $dir/lib; then LDFLAGS="$LDFLAGS -L$dir/lib"; LIBICONV="-L$dir/lib"; fi
    done
   ])

  AC_CACHE_CHECK(for iconv, am_cv_func_iconv, [
    am_cv_func_iconv="no, consider installing GNU libiconv"
    am_cv_lib_iconv=no
    AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
      [iconv_t cd = iconv_open("","");
       iconv(cd,NULL,NULL,NULL,NULL);
       iconv_close(cd);],
      am_cv_func_iconv=yes)
    if test "$am_cv_func_iconv" != yes; then
      am_save_LIBS="$LIBS"
      LIBS="$LIBS -liconv"
      AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
        [iconv_t cd = iconv_open("","");
         iconv(cd,NULL,NULL,NULL,NULL);
         iconv_close(cd);],
        am_cv_lib_iconv=yes
        am_cv_func_iconv=yes)
      LIBS="$am_save_LIBS"
    fi
  ])
  if test "$am_cv_func_iconv" = yes; then
    AC_DEFINE(HAVE_ICONV, 1, [Define if you have the iconv() function.])
    AC_MSG_CHECKING([for iconv declaration])
    AC_CACHE_VAL(am_cv_proto_iconv, [
      AC_TRY_COMPILE([
#include <stdlib.h>
#include <iconv.h>
extern
#ifdef __cplusplus
"C"
#endif
#if defined(__STDC__) || defined(__cplusplus)
size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
#else
size_t iconv();
#endif
], [], am_cv_proto_iconv_arg1="", am_cv_proto_iconv_arg1="const")
      am_cv_proto_iconv="extern size_t iconv (iconv_t cd, $am_cv_proto_iconv_arg1 char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);"])
    am_cv_proto_iconv=`echo "[$]am_cv_proto_iconv" | tr -s ' ' | sed -e 's/( /(/'`
    AC_MSG_RESULT([$]{ac_t:-
         }[$]am_cv_proto_iconv)
    AC_DEFINE_UNQUOTED(ICONV_CONST, $am_cv_proto_iconv_arg1,
      [Define as const if the declaration of iconv() needs const.])
  fi
  if test "$am_cv_lib_iconv" = yes; then
    LIBICONV="$LIBICONV -liconv"
  else
    LIBICONV=
  fi
  LDFLAGS="$save_LDFLAGS"
  AC_SUBST(LIBICONV)
])


# OpenSSL check

AC_DEFUN([CHECK_OPENSSL],
[AC_MSG_CHECKING(if openssl is wanted)
AC_ARG_WITH(openssl, AC_HELP_STRING([--with-openssl], [--with-openssl=DIR build with OpenSSL (license NOT compatible cf. User Guide)])
,[ AC_MSG_RESULT(yes)
    for dir in $withval /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/local /usr; do
        ssldir="$dir"
        if test -f "$dir/include/openssl/ssl.h"; then
            found_ssl="yes"
            CFLAGS="$CFLAGS -I$ssldir/include"
            break
        fi
    done
    if test x$found_ssl != xyes; then
        AC_MSG_ERROR(Cannot find OpenSSL libraries)
    else
        echo "OpenSSL found in $ssldir"
        NETWORK_LIBS="$NETWORK_LIBS -lssl -lcrypto"
        LDFLAGS="$LDFLAGS -L$ssldir/lib"
        HAVE_OPENSSL=yes
        AC_DEFINE(HAVE_OPENSSL, 1, [Define if you have the OpenSSL.])
    fi
    AC_SUBST(HAVE_OPENSSL)
 ],[
    AC_MSG_RESULT(no)
 ])
])


##
# Found on autoconf archive
# Based on Caolan McNamara's gethostbyname_r macro. 
# Based on David Arnold's autoconf suggestion in the threads faq.

AC_DEFUN([AC_raf_FUNC_WHICH_GETSERVBYNAME_R],
[ac_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $NETWORK_LIBS"
AC_CACHE_CHECK(for which type of getservbyname_r, ac_cv_func_which_getservbyname_r, [
        AC_TRY_LINK([
#               include <netdb.h>
        ],      [

        char *name;
        char *proto;
        struct servent *se;
        struct servent_data data;
        (void) getservbyname_r(name, proto, se, &data);

                ],ac_cv_func_which_getservbyname_r=four,
                        [
  AC_TRY_LINK([
#   include <netdb.h>
  ], [
        char *name;
        char *proto;
        struct servent *se, *res;
        char buffer[2048];
        int buflen = 2048;
        (void) getservbyname_r(name, proto, se, buffer, buflen, &res)
  ],ac_cv_func_which_getservbyname_r=six,

  [
  AC_TRY_LINK([
#   include <netdb.h>
  ], [
        char *name;
        char *proto;
        struct servent *se;
        char buffer[2048];
        int buflen = 2048;
        (void) getservbyname_r(name, proto, se, buffer, buflen)
  ],ac_cv_func_which_getservbyname_r=five,ac_cv_func_which_getservbyname_r=no)

  ]

  )
                        ]
                )])

if test $ac_cv_func_which_getservbyname_r = six; then
  AC_DEFINE(HAVE_FUNC_GETSERVBYNAME_R_6, 1, [Define to 1 if your system provides the 6-parameter version of getservbyname_r().])
elif test $ac_cv_func_which_getservbyname_r = five; then
  AC_DEFINE(HAVE_FUNC_GETSERVBYNAME_R_5, 1, [Define to 1 if your system provides the 5-parameter version of getservbyname_r().])
elif test $ac_cv_func_which_getservbyname_r = four; then
  AC_DEFINE(HAVE_FUNC_GETSERVBYNAME_R_4, 1, [Define to 1 if your system provides the 4-parameter version of getservbyname_r().])

fi
CFLAGS=$ac_save_CFLAGS
])

##
# @synopsis AC_caolan_FUNC_WHICH_GETHOSTBYNAME_R
#
# Provides a test to determine the correct 
# way to call gethostbyname_r
#
# defines HAVE_FUNC_GETHOSTBYNAME_R_6 if it needs 6 arguments (e.g linux)
# defines HAVE_FUNC_GETHOSTBYNAME_R_5 if it needs 5 arguments (e.g. solaris)
# defines HAVE_FUNC_GETHOSTBYNAME_R_3 if it needs 3 arguments (e.g. osf/1)
#
# if used in conjunction in gethostname.c the api demonstrated
# in test.c can be used regardless of which gethostbyname_r 
# exists. These example files found at
# http://www.csn.ul.ie/~caolan/publink/gethostbyname_r
#
# @version $Id: acinclude.m4,v 1.34 2006-01-25 14:02:32 freddy77 Exp $
# @author Caolan McNamara <caolan@skynet.ie>
#
# based on David Arnold's autoconf suggestion in the threads faq
##
AC_DEFUN([AC_caolan_FUNC_WHICH_GETHOSTBYNAME_R],
[ac_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $NETWORK_LIBS"
AC_CACHE_CHECK(for which type of gethostbyname_r, ac_cv_func_which_gethostname_r, [
	AC_TRY_LINK([
#		include <netdb.h> 
  	], 	[

        char *name;
        struct hostent *he;
        struct hostent_data data;
        (void) gethostbyname_r(name, he, &data);

		],ac_cv_func_which_gethostname_r=three, 
			[
dnl			ac_cv_func_which_gethostname_r=no
  AC_TRY_LINK([
#   include <netdb.h>
  ], [
	char *name;
	struct hostent *he, *res;
	char buffer[2048];
	int buflen = 2048;
	int h_errnop;
	(void) gethostbyname_r(name, he, buffer, buflen, &res, &h_errnop)
  ],ac_cv_func_which_gethostname_r=six,
  
  [
dnl  ac_cv_func_which_gethostname_r=no
  AC_TRY_LINK([
#   include <netdb.h>
  ], [
			char *name;
			struct hostent *he;
			char buffer[2048];
			int buflen = 2048;
			int h_errnop;
			(void) gethostbyname_r(name, he, buffer, buflen, &h_errnop)
  ],ac_cv_func_which_gethostname_r=five,ac_cv_func_which_gethostname_r=no)

  ]
  
  )
			]
		)])

if test $ac_cv_func_which_gethostname_r = six; then
  AC_DEFINE(HAVE_FUNC_GETHOSTBYNAME_R_6, 1, [Define to 1 if your system provides the 6-parameter version of gethostbyname_r().])
elif test $ac_cv_func_which_gethostname_r = five; then
  AC_DEFINE(HAVE_FUNC_GETHOSTBYNAME_R_5, 1, [Define to 1 if your system provides the 5-parameter version of gethostbyname_r().])
elif test $ac_cv_func_which_gethostname_r = three; then
  AC_DEFINE(HAVE_FUNC_GETHOSTBYNAME_R_3, 1, [Define to 1 if your system provides the 3-parameter version of gethostbyname_r().])

fi
CFLAGS=$ac_save_CFLAGS
])


##
# based on gethostbyname_r check and snippits from curl's check
##
AC_DEFUN([AC_tds_FUNC_WHICH_GETHOSTBYADDR_R],
[ac_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $NETWORK_LIBS"
AC_CACHE_CHECK(for which type of gethostbyaddr_r, ac_cv_func_which_gethostbyaddr_r, [
	AC_TRY_LINK([
#include <sys/types.h>
#include <netdb.h>
  	], 	[
char * address;
int length;
int type;
struct hostent h;
struct hostent_data hdata;
int rc;
rc = gethostbyaddr_r(address, length, type, &h, &hdata);

],ac_cv_func_which_gethostbyaddr_r=five, 
  [
dnl			ac_cv_func_which_gethostbyaddr_r=no
  AC_TRY_LINK([
#include <sys/types.h>
#include <netdb.h>
  ], [
char * address;
int length;
int type;
struct hostent h;
char buffer[8192];
int h_errnop;
struct hostent * hp;

hp = gethostbyaddr_r(address, length, type, &h,
                     buffer, 8192, &h_errnop);

],ac_cv_func_which_gethostbyaddr_r=seven,
  
 [
dnl  ac_cv_func_which_gethostbyaddr_r=no
  AC_TRY_LINK([
#include <sys/types.h>
#include <netdb.h>
  ], [
char * address;
int length;
int type;
struct hostent h;
char buffer[8192];
int h_errnop;
struct hostent * hp;
int rc;

rc = gethostbyaddr_r(address, length, type, &h,
                     buffer, 8192, &hp, &h_errnop);

],ac_cv_func_which_gethostbyaddr_r=eight,ac_cv_func_which_gethostbyaddr_r=no)

]
  )
			]
		)])

if test $ac_cv_func_which_gethostbyaddr_r = eight; then
  AC_DEFINE(HAVE_FUNC_GETHOSTBYADDR_R_8, 1, [Define to 1 if your system provides the 8-parameter version of gethostbyaddr_r().])
elif test $ac_cv_func_which_gethostbyaddr_r = seven; then
  AC_DEFINE(HAVE_FUNC_GETHOSTBYADDR_R_7, 1, [Define to 1 if your system provides the 7-parameter version of gethostbyaddr_r().])
elif test $ac_cv_func_which_gethostbyaddr_r = five; then
  AC_DEFINE(HAVE_FUNC_GETHOSTBYADDR_R_5, 1, [Define to 1 if your system provides the 5-parameter version of gethostbyaddr_r().])

fi
CFLAGS=$ac_save_CFLAGS
])

AC_DEFUN([AC_HAVE_MALLOC_OPTIONS],
 [AC_CACHE_CHECK([whether malloc_options variable is present],
   ac_cv_have_malloc_options,
   [AC_TRY_LINK([
#include <stdlib.h>
      ],[
extern char *malloc_options;
malloc_options = "AJR";
      ],
     ac_cv_have_malloc_options=yes,
     ac_cv_have_malloc_options=no)])
  if test $ac_cv_have_malloc_options = yes; then
   AC_DEFINE(HAVE_MALLOC_OPTIONS, 1, [Define to 1 if your system provides the malloc_options variable.])
  fi])

##
# Check getpwuid_r parameters
# There are three version of this function
#   int  getpwuid_r(uid_t uid, struct passwd *result, char *buffer, int buflen);
#   (hp/ux 10.20, digital unix 4)
#   struct passwd *getpwuid_r(uid_t uid, struct passwd * pwd, char *buffer, int buflen);
#   (SunOS 5.5, many other)
#   int  getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer, size_t buflen, struct passwd **result);
#   (hp/ux 11, many other, posix compliant)
##
AC_DEFUN([AC_tds_FUNC_WHICH_GETPWUID_R],
[if test x$ac_cv_func_getpwuid = xyes; then
AC_CACHE_CHECK(for which type of getpwuid_r, ac_cv_func_which_getpwuid_r, [
AC_TRY_COMPILE([
#include <unistd.h>
#include <pwd.h>
  ], [
struct passwd bpw;
char buf[1024];
char *dir = getpwuid_r(getuid(), &bpw, buf, sizeof(buf))->pw_dir;
],ac_cv_func_which_getpwuid_r=four_pw,
[AC_TRY_RUN([
#include <unistd.h>
#include <pwd.h>
int main() {
struct passwd bpw;
char buf[1024];
getpwuid_r(getuid(), &bpw, buf, sizeof(buf));
return 0;
}
],ac_cv_func_which_getpwuid_r=four, 
  [AC_TRY_RUN([
#include <unistd.h>
#include <pwd.h>
int main() {
struct passwd *pw, bpw;
char buf[1024];
getpwuid_r(getuid(), &bpw, buf, sizeof(buf), &pw);
return 0;
}
],ac_cv_func_which_getpwuid_r=five,
ac_cv_func_which_getpwuid_r=no)]
)]
)])

if test $ac_cv_func_which_getpwuid_r = four_pw; then
  AC_DEFINE(HAVE_FUNC_GETPWUID_R_4, 1, [Define to 1 if your system provides the 4-parameter version of getpwuid_r().])
  AC_DEFINE(HAVE_FUNC_GETPWUID_R_4_PW, 1, [Define to 1 if your system getpwuid_r() have 4 parameters and return struct passwd*.])
elif test $ac_cv_func_which_getpwuid_r = four; then
  AC_DEFINE(HAVE_FUNC_GETPWUID_R_4, 1, [Define to 1 if your system provides the 4-parameter version of getpwuid_r().])
elif test $ac_cv_func_which_getpwuid_r = five; then
  AC_DEFINE(HAVE_FUNC_GETPWUID_R_5, 1, [Define to 1 if your system provides the 5-parameter version of getpwuid_r().])
fi

fi
])

AC_DEFUN([AC_tds_FUNC_WHICH_LOCALTIME_R],
[AC_CACHE_CHECK(for which type of localtime_r, ac_cv_func_which_localtime_r, [
	AC_TRY_COMPILE([
#include <unistd.h>
#include <time.h>
  	], 	[
struct tm mytm;
time_t t;
int y = localtime_r(&t, &mytm)->tm_year;
],ac_cv_func_which_localtime_r=struct,
  ac_cv_func_which_localtime_r=int)
])

if test $ac_cv_func_which_localtime_r = struct; then
  AC_DEFINE(HAVE_FUNC_LOCALTIME_R_TM, 1, [Define to 1 if your localtime_r return a struct tm*.])
else
  AC_DEFINE(HAVE_FUNC_LOCALTIME_R_INT, 1, [Define to 1 if your localtime_r return a int.])
fi
])

##
# This macro came from internet, appear in lftp, rsync and others.
##
AC_DEFUN([TYPE_SOCKLEN_T],
[
  AC_CHECK_TYPE([socklen_t], ,[
    AC_MSG_CHECKING([for socklen_t equivalent])
    AC_CACHE_VAL([xml_cv_socklen_t_equiv],
    [
      # Systems have either "struct sockaddr *" or
      # "void *" as the second argument to getpeername
      xml_cv_socklen_t_equiv=
      for arg2 in "struct sockaddr" void; do
        for t in int size_t unsigned long "unsigned long"; do
          AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_WINSOCK2_H
# include <winsock2.h>
int PASCAL getpeername (SOCKET, $arg2 *, $t *);
#elif defined(HAVE_SYS_SOCKET_H)
# include <sys/socket.h>
int getpeername (int, $arg2 *, $t *);
#endif

          ],[
            $t len;
            getpeername(0,0,&len);
          ],[
            xml_cv_socklen_t_equiv="$t"
            break
          ])
        done
      done

      if test "x$xml_cv_socklen_t_equiv" = x; then
        AC_MSG_ERROR([Cannot find a type to use in place of socklen_t])
      fi
    ])
    AC_MSG_RESULT($xml_cv_socklen_t_equiv)
    AC_DEFINE_UNQUOTED(socklen_t, $xml_cv_socklen_t_equiv,
                      [type to use in place of socklen_t if not defined])],
    [#include <sys/types.h>
#include <sys/socket.h>])
])

##
# Test for 64bit integer sprintf format specifier
##
AC_DEFUN([SPRINTF_I64_FORMAT],
[tds_i64_format=
for arg in ld lld I64d; do
	AC_TRY_RUN([
#include <stdio.h>
#include <string.h>
int main() {
char buf[20];
$tds_sysdep_int64_type ll = ((($tds_sysdep_int64_type) 0x12345) << 32) + 0x6789abcd;
sprintf(buf, "%$arg", ll);
return strcmp(buf, "320255973501901") != 0;
}
],tds_i64_format=$arg)
	if test "x$tds_i64_format" != x; then
		AC_DEFINE_UNQUOTED(TDS_I64_FORMAT, ["$tds_i64_format"], [define to format string used for 64bit integers])
		break;
	fi
done
])

