#serial AM2

AC_DEFUN(AC_HAVE_INADDR_NONE,
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




dnl From Bruno Haible.

AC_DEFUN([AM_ICONV],
[
  dnl Some systems have iconv in libc, some have it in libiconv (OSF/1 and
  dnl those with the standalone portable GNU libiconv installed).

  AC_ARG_WITH([libiconv-prefix],
[  --with-libiconv-prefix=DIR  search for libiconv in DIR/include and DIR/lib], [
    for dir in `echo "$withval" | tr : ' '`; do
      if test -d $dir/include; then CPPFLAGS="$CPPFLAGS -I$dir/include"; fi
      if test -d $dir/lib; then LDFLAGS="$LDFLAGS -L$dir/lib"; fi
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
  LIBICONV=
  if test "$am_cv_lib_iconv" = yes; then
    LIBICONV="-liconv"
  fi
  AC_SUBST(LIBICONV)
])


dnl Found on autoconf archive
dnl Based on Caolan McNamara's gethostbyname_r macro. 
dnl Based on David Arnold's autoconf suggestion in the threads faq.

AC_DEFUN([AC_raf_FUNC_WHICH_GETSERVBYNAME_R],
[ac_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $NETWORK_LIBS"
AC_CACHE_CHECK(for which type of getservbyname_r, ac_cv_func_which_getservbyname_r, [
        AC_TRY_COMPILE([
#               include <netdb.h>
        ],      [

        char *name;
        char *proto;
        struct servent *se;
        struct servent_data data;
        (void) getservbyname_r(name, proto, se, &data);

                ],ac_cv_func_which_getservbyname_r=four,
                        [
  AC_TRY_COMPILE([
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
  AC_TRY_COMPILE([
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


dnl @synopsis AC_caolan_FUNC_WHICH_GETHOSTBYNAME_R
dnl
dnl Provides a test to determine the correct 
dnl way to call gethostbyname_r
dnl
dnl defines HAVE_FUNC_GETHOSTBYNAME_R_6 if it needs 6 arguments (e.g linux)
dnl defines HAVE_FUNC_GETHOSTBYNAME_R_5 if it needs 5 arguments (e.g. solaris)
dnl defines HAVE_FUNC_GETHOSTBYNAME_R_3 if it needs 3 arguments (e.g. osf/1)
dnl
dnl if used in conjunction in gethostname.c the api demonstrated
dnl in test.c can be used regardless of which gethostbyname_r 
dnl exists. These example files found at
dnl http://www.csn.ul.ie/~caolan/publink/gethostbyname_r
dnl
dnl @version $Id: acinclude.m4,v 1.18 2003-05-06 11:13:41 freddy77 Exp $
dnl @author Caolan McNamara <caolan@skynet.ie>
dnl
dnl based on David Arnold's autoconf suggestion in the threads faq
dnl
AC_DEFUN(AC_caolan_FUNC_WHICH_GETHOSTBYNAME_R,
[ac_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $NETWORK_LIBS"
AC_CACHE_CHECK(for which type of gethostbyname_r, ac_cv_func_which_gethostname_r, [
	AC_TRY_COMPILE([
#		include <netdb.h> 
  	], 	[

        char *name;
        struct hostent *he;
        struct hostent_data data;
        (void) gethostbyname_r(name, he, &data);

		],ac_cv_func_which_gethostname_r=three, 
			[
dnl			ac_cv_func_which_gethostname_r=no
  AC_TRY_COMPILE([
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
  AC_TRY_COMPILE([
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


dnl based on gethostbyname_r check and snippits from curl's check

AC_DEFUN(AC_tds_FUNC_WHICH_GETHOSTBYADDR_R,
[ac_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $NETWORK_LIBS"
AC_CACHE_CHECK(for which type of gethostbyaddr_r, ac_cv_func_which_gethostbyaddr_r, [
	AC_TRY_COMPILE([
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
  AC_TRY_COMPILE([
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
  AC_TRY_COMPILE([
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
  AC_DEFINE(HAVE_FUNC_GETHOSTBYADDR_R_7, 1, [Define to 1 if your system provides the 6-parameter version of gethostbyaddr_r().])
elif test $ac_cv_func_which_gethostbyaddr_r = five; then
  AC_DEFINE(HAVE_FUNC_GETHOSTBYADDR_R_5, 1, [Define to 1 if your system provides the 5-parameter version of gethostbyaddr_r().])

fi
CFLAGS=$ac_save_CFLAGS
])

AC_DEFUN(AC_HAVE_MALLOC_OPTIONS,
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

dnl Check getpwuid_r parameters
dnl There are three version of this function
dnl   int  getpwuid_r(uid_t uid, struct passwd *result, char *buffer, int buflen);
dnl   (hp/ux 10.20, digital unix 4)
dnl   struct passwd *getpwuid_r(uid_t uid, struct passwd * pwd, char *buffer, int buflen);
dnl   (SunOS 5.5, many other)
dnl   int  getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer, size_t buflen, struct passwd **result);
dnl   (hp/ux 11, many other, posix compliant)

AC_DEFUN(AC_tds_FUNC_WHICH_GETPWUID_R,
[AC_CACHE_CHECK(for which type of getpwuid_r, ac_cv_func_which_getpwuid_r, [
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
])

AC_DEFUN(AC_tds_FUNC_WHICH_LOCALTIME_R,
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

dnl TDS_ICONV_CHARSET VAR RES LIST
AC_DEFUN(TDS_ICONV_CHARSET,
[tds_charset=
for TDSTO in $3
do
	RES=`(eval $tds_iconvtest) 2> /dev/null`
	if test "X$RES" = "X$2"; then
		tds_charset=$TDSTO
		break
	fi
done
if test "X$tds_charset" != "X"; then
AC_DEFINE(HAVE_CHARSET_$1, 1, [Define to 1 if iconv support $1 charset.])
AC_DEFINE_UNQUOTED(CHARSET_$1, ["$tds_charset"], [Define to the iconv name of $1 charset.])
fi
])

AC_DEFUN(TDS_ICONV,
[
dnl does cksum use tab or space (Solaris use space)
SUM=`true | cksum | cut -f 1`
if test "X$SUM" = "X4294967295"; then
	tds_iconvtest='echo $TDSSTR | iconv -f $TDSFROM -t $TDSTO | cksum | cut -f 1'
else
	SUM=`true | cksum | cut -d " " -f 1`
	if test "X$SUM" = "X4294967295"; then
		tds_iconvtest='echo $TDSSTR | iconv -f $TDSFROM -t $TDSTO | cksum | cut -d " " -f 1'
	fi
fi

dnl test for ucs2 and iso8859-1 (types always presents)
TDSSTR=foo
ICONV_ISO1=
ICONV_UCS2=
for TDSFROM in "ISO8859-1" "ISO-8859-1" "iso8859-1" "iso88591" "iso8859_1"
do
        for TDSTO in "UCS-2" "UCS2" "ucs2"
        do
                RES=`(eval $tds_iconvtest) 2> /dev/null`
                if test "X$RES" = "X3637111430" -o "X$RES" = "X823496052"; then
			if test "X$ICONV_ISO1" = "X"; then
				ICONV_ISO1=$TDSFROM
			fi
			if test "X$ICONV_UCS2" = "X"; then
				ICONV_UCS2=$TDSTO
			fi
		fi
		if test "X$ICONV_ISO1" != "X" -a "X$ICONV_UCS2" != "X"; then
			break 2
		fi
        done
done

AC_DEFINE(HAVE_CHARSET_ISO1, 1, [Define to 1 if iconv support ISO1 charset.])
AC_DEFINE_UNQUOTED(CHARSET_ISO1, ["$ICONV_ISO1"], [Define to the iconv name of ISO1 charset.])
AC_DEFINE(HAVE_CHARSET_UCS2, 1, [Define to 1 if iconv support UCS2 charset.])
AC_DEFINE_UNQUOTED(CHARSET_UCS2, ["$ICONV_UCS2"], [Define to the iconv name of UCS2 charset.])

# now we have a type we can check for others names
TDSFROM=$ICONV_ISO1
TDS_ICONV_CHARSET(UTF8, 3915528286, ["UTF-8" "UTF8" "utf8"])
TDS_ICONV_CHARSET(UCS2LE, 3637111430, ["$ICONV_UCS2" "UCS2LE" "UCS-2LE" "ucs2le"])
TDS_ICONV_CHARSET(UCS4LE, 2049347138, ["UCS4" "UCS-4" "UCS4LE" "UCS-4LE"])
TDS_ICONV_CHARSET(UCS4BE, 991402119, ["UCS4" "UCS-4" "UCS4BE" "UCS-4BE"])

])

