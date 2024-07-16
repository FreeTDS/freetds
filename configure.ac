dnl Process this file with autoconf to produce a configure script.

dnl ------------------------------------------------------------
dnl If you're trying to create a new configure test, try
dnl
dnl 	http://autogen.sourceforge.net/conftest.html
dnl	http://autoconf-archive.cryp.to/
dnl
dnl ------------------------------------------------------------

# ------------------------------------------------------------
# Initialization
# ------------------------------------------------------------
AC_INIT(FreeTDS, 1.5.dev.esyscmd(printf $(date +"%Y%m%d")))
AC_CONFIG_SRCDIR(src/dblib/dblib.c)
AC_PREREQ(2.53)

AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([dist-bzip2 parallel-tests subdir-objects foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES(yes)])
AC_CONFIG_HEADERS(include/config.h)
AC_CONFIG_MACRO_DIR([m4])

dnl configuration directory will be /usr/local/etc
AC_PREFIX_DEFAULT(/usr/local)

dnl This disable address sanitizer leak detector as
dnl some checks cause leaks which are detected as errors
ASAN_OPTIONS='detect_leaks=0'
export ASAN_OPTIONS

dnl 0.X.Y -> 0.X.Y
dnl 0.X.dev.date -> 0.(X-1).9999
dnl 0.XrcY -> 0.(X-1).9999
dnl dev.0.X.Y -> 0.X.Y
ver=[`echo $VERSION.0 | sed 's,^dev\.,,; s,^\([0-9][0-9]*\.[0-9][0-9]*\)rc,\1.dev.,g; s/\.dev\./.9999./'`]
match=['^\([0-9][0-9]*\)\.\([0-9][0-9]*\)\.\([0-9][0-9]*\).*']
if test "`echo $ver | sed \"s,$match,,\"`" != ""; then
	AC_MSG_ERROR([Invalid version $VERSION specified])
fi
MAJOR=[`echo $ver | sed "s,$match,\1,"`]
MINOR=[`echo $ver | sed "s,$match,\2,"`]
SUBVERSION=[`echo $ver | sed "s,$match,\3,"`]
BUILD_NUMBER="`date +1%j` - 1000 + ( `date +%Y` - 2023 )"
BUILD_NUMBER="`expr $BUILD_NUMBER \* 366`"
if test "$SUBVERSION" = "9999"; then
	MINOR=`expr $MINOR - 1`
fi
AC_SUBST(MAJOR)
AC_SUBST(MINOR)
AC_SUBST(SUBVERSION)
AC_SUBST(BUILD_NUMBER)

AH_TOP([#define _freetds_config_h_])

# ------------------------------------------------------------
# Commands and Utilities
# ------------------------------------------------------------
AC_PROG_AWK
AC_PROG_GREP
AC_PROG_INSTALL
AC_PROG_MKDIR_P
AC_PROG_LN_S
AC_PROG_SED
PKG_PROG_PKG_CONFIG

# ------------------------------------------------------------
# Host-specific configuration
# ------------------------------------------------------------
AC_CANONICAL_HOST
netdb_reentrant=no
case $host in
  *-*-osf*)
    CPPFLAGS="$CPPFLAGS -D_OSF_SOURCE -D_XOPEN_SOURCE_EXTENDED -D_POSIX_C_SOURCE=199506L"
    netdb_reentrant=yes
    ;;
  *-*-hpux11*)
    netdb_reentrant=yes
    ;;
  *-*-cygwin*)
    netdb_reentrant=yes
    ;;
esac

case $host_os in
  darwin* | rhapsody*)
    AM_CONDITIONAL(MACOSX, true)
    ;;
  *)
    AM_CONDITIONAL(MACOSX, false)
    ;;
esac

if test $netdb_reentrant = yes; then
	AC_DEFINE(NETDB_REENTRANT, 1, [Define to 1 if the BSD-style netdb interface is reentrant.])
fi

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

# ------------------------------------------------------------
# Checks for programs.
# ------------------------------------------------------------
AC_PROG_CC
AC_PROG_CPP
AM_PROG_CC_C_O
AC_C_INLINE

dnl Disable libtool 1.5 support for languages we don't use
dnl (cfr http://lists.gnu.org/archive/html/libtool/2005-08/msg00137.html)
define([AC_LIBTOOL_LANG_CXX_CONFIG], [:])dnl
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
define([AC_LIBTOOL_LANG_GCJ_CONFIG], [:])dnl

LT_INIT([win32-dll])
LT_PROG_RC

AC_ARG_ENABLE(libiconv,
	AS_HELP_STRING([--disable-libiconv], [do not attempt to include libiconv]),
	[use_libiconv=$enableval], [use_libiconv=yes])

if test "$use_libiconv" = "yes" ; then
	AM_ICONV
else
	AC_MSG_NOTICE(libiconv disabled)
	AC_DEFINE_UNQUOTED(ICONV_CONST, const, [Define as const if the declaration of iconv() needs const.])
fi


# See if we have doxygen installed
AC_CHECK_PROG(DOXYGEN,doxygen,doxygen)
AM_CONDITIONAL(HAVE_DOXYGEN, [test -n "$DOXYGEN"])
AC_CHECK_PROG(ODBC_CONFIG,odbc_config,odbc_config)

# ------------------------------------------------------------
# Check for git-only sources.
# ------------------------------------------------------------
#
# Distribution tarballs include some generated files.  These
# files are not maintained in git, but their bases are, and
# the bases are not in the tarball.  For example, num_limits.h
# is distributed, but it is generated from numeric.pl, which
# is kept in git.
# If the base is found, we use it as a source in the Makefile,
# otherwise the Makefile will (correctly) assume that the
# generated file is already present.
#
if test -f "${srcdir}/src/tds/num_limits.pl"; then
	AM_CONDITIONAL(HAVE_PERL_SOURCES, true)
	AC_PATH_PROG(GPERF, gperf)
	AS_IF([test "x$GPERF" = x], [AC_MSG_ERROR([required program 'gperf' not found.])])
else
	AM_CONDITIONAL(HAVE_PERL_SOURCES, false)
fi
AC_SUBST(HAVE_PERL_SOURCES)


# ------------------------------------------------------------
# Checks for libraries.
# ------------------------------------------------------------
AC_ARG_ENABLE(krb5,
	AS_HELP_STRING([--enable-krb5@<:@=LIB@:>@], [enable Kerberos support, optionally with library]))

krb5_libs=auto
case $enable_krb5 in
  yes) ;;
  "" | no) krb5_libs= ;;
  -* | */* | *.a | *.so | *.so.* | *.o) krb5_libs="$enable_krb5" ;;
  *) krb5_libs="-l$enable_krb5" ;;
esac

ACX_PUSH_LIBS("")
tds_mingw=no
case $host in
*-*-mingw*)
	tds_mingw=yes
	if test "$host_cpu" = "x86_64"; then
		LIBS="-lws2_32"
	elif test -r /usr/lib/w32api/libws2_32.a; then
		LIBS="-L/usr/lib/w32api -lws2_32"
	else
		LIBS="-lws2_32"
	fi
	LIBS="$LIBS -lcrypt32"
	krb5_libs=
	AM_CONDITIONAL(MINGW32, true)
	;;
*-*-hpux*)
	# these lines are needed to fix a problem for HP-UX
	# HP-UX define two versions of sockets, one BSD and one X/Open
	# these versions are not binary compatible (BSD use int where X/Open
	# use socklen_t == size_t) and different libraries (BSD in libc and
	# X/Open in libxnet). X/Open is used if _XOPEN_SOURCE and
	# _XOPEN_SOURCE_EXTENDED are defined. To complicate the things gcc
	# by default define _XOPEN_SOURCE_EXTENDED so define always both
	# constants and link always libxnet!
	CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED"
	LIBS="$LIBS -lxnet"
	AC_SEARCH_LIBS(gethostbyname, nsl)

	# check for _xpg_ functions
	AC_CHECK_FUNCS([_xpg_accept _xpg_getpeername _xpg_getsockname _xpg_getsockopt _xpg_recvfrom \
	__accept __getpeername __getsockname __getsockopt __recvfrom])
	AM_CONDITIONAL(MINGW32, false)
	;;
*)
	AC_SEARCH_LIBS(socket, socket)
	AC_SEARCH_LIBS(gethostbyname, nsl)

	AM_CONDITIONAL(MINGW32, false)
	;;
esac

case "$krb5_libs" in
"" ) ;;
"auto")
	AC_SEARCH_LIBS(gss_init_sec_context, [gssapi_krb5 gssapi])
	if test "$ac_cv_search_gss_init_sec_context" != no; then
		AC_DEFINE(ENABLE_KRB5, 1, [Defined if --enable-krb5 used and library detected])
	fi
	;;
*)
	AC_DEFINE(ENABLE_KRB5, 1, [Defined if --enable-krb5 used and library detected])
	LIBS="$LIBS $krb5_libs"
	;;
esac

if test -n "$krb5_libs"; then
	for d in /usr/include/heimdal /usr/include/krb5 /usr/include/et; do
		if test -d "$d"; then
			CPPFLAGS="$CPPFLAGS -I$d"
		fi
	done
	AC_CHECK_FUNCS([error_message])
fi

NETWORK_LIBS="$LIBS"
ACX_POP_LIBS
AC_SUBST(NETWORK_LIBS)

if test $tds_mingw = no; then
	# readline test
	# Readline is needed by the tty session. Set up a special READLINE_LIBS
	# substitution for it.
	# Readline is linked with curses, and on some systems termcap must be
	# linked in. Others (inc debian) have termcap built into ncurses.
	ACX_PUSH_LIBS("")
	AC_SEARCH_LIBS(tgetent, [readline ncurses curses termcap])
	AC_CHECK_LIB([readline], [readline], [LIBS="-lreadline $LIBS"
		AC_DEFINE(HAVE_READLINE, 1, [Define to 1 if you have the GNU Readline library.])], [LIBS=""] )
	AC_CHECK_FUNCS([rl_on_new_line rl_reset_line_state])
	AC_LINK_IFELSE([AC_LANG_SOURCE([[

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif /* HAVE_READLINE */

int main()
{
	rl_inhibit_completion = 1;
	return 0;
}
]])],AC_DEFINE(HAVE_RL_INHIBIT_COMPLETION, 1, [Define to 1 if you have rl_inhibit_completion.]))
	READLINE_LIBS="$LIBS"
	ACX_POP_LIBS
	AC_SUBST(READLINE_LIBS)

	AX_PTHREAD
	AM_CFLAGS="$AM_CFLAGS $PTHREAD_CFLAGS"
	AC_SUBST(AM_CFLAGS)
	LIBS="$PTHREAD_LIBS $LIBS"
	AC_SEARCH_LIBS(pthread_condattr_setclock, [rt posix4 pthread])
	AC_CHECK_FUNCS([pthread_condattr_setclock pthread_cond_timedwait_relative_np])
fi

# check for SSPI
AC_ARG_ENABLE(sspi,
	AS_HELP_STRING([--disable-sspi], [disable SSPI support]))
# disable by default
enable_sspi=${enable_sspi-yes}
# only if MingW (TODO other compilers)
test $tds_mingw != yes && enable_sspi=no
AM_CONDITIONAL(HAVE_SSPI, test "$enable_sspi" = "yes")
if test "$enable_sspi" = "yes"; then
	AC_DEFINE(HAVE_SSPI, 1, [Defined if not --disable-sspi and SSPI detected])
fi

# ------------------------------------------------------------
# Checks for header files.
# ------------------------------------------------------------
AC_STRUCT_TIMEZONE
AC_CHECK_MEMBERS([struct tm.__tm_zone],,,[#include <sys/types.h>
#include <$ac_cv_struct_tm>
])
AC_CHECK_HEADERS([errno.h libgen.h \
	limits.h locale.h poll.h \
	signal.h stddef.h \
	sys/param.h sys/select.h sys/stat.h \
	sys/time.h sys/types.h sys/resource.h \
	sys/eventfd.h \
	sys/wait.h unistd.h netdb.h \
	wchar.h inttypes.h winsock2.h \
	localcharset.h valgrind/memcheck.h malloc.h dirent.h \
	stdbool.h gnutls/abstract.h getopt.h fcntl.h])
AC_CHECK_HEADERS([windows.h sql.h odbcss.h],[],[],[[#ifdef HAVE_WINDOWS_H
# include <windows.h>
#endif
#ifdef HAVE_SQL_H
# include <sql.h>
#endif
]])
AM_CONDITIONAL(HAVE_ODBCSS_H, test "$ac_cv_header_odbcss_h" = "yes")
if test $tds_mingw = no; then
	AC_CHECK_HEADERS([arpa/inet.h \
			langinfo.h \
			netdb.h \
			netinet/in.h \
			netinet/tcp.h \
			roken.h \
			com_err.h \
			paths.h \
			sys/ioctl.h \
			sys/socket.h ])
fi
AC_HAVE_INADDR_NONE

# ------------------------------------------------------------
# Checks for integers/floats of different sizes
# ------------------------------------------------------------
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)
AC_CHECK_SIZEOF(long double)
AC_CHECK_SIZEOF(long long)
# This will define SIZEOF_LONG_INT, used by some ODBC Driver Manager
AC_CHECK_SIZEOF(long int)
AC_CHECK_SIZEOF(__int64)
AC_CHECK_SIZEOF(__int128)
AC_CHECK_SIZEOF(void *)
AC_CHECK_SIZEOF(wchar_t,,[$ac_includes_default
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif])

case 2 in
$ac_cv_sizeof_short)   tds_sysdep_int16_type=short;;
$ac_cv_sizeof_int)     tds_sysdep_int16_type=int;;
*)                     AC_MSG_ERROR(No 16-bit int found.)
esac

case 4 in
$ac_cv_sizeof_short)   tds_sysdep_int32_type=short;;
$ac_cv_sizeof_int)     tds_sysdep_int32_type=int;;
$ac_cv_sizeof_long)    tds_sysdep_int32_type=long;;
*)                     AC_MSG_ERROR(No 32-bit int found.)
esac

case 4 in
$ac_cv_sizeof_float)       tds_sysdep_real32_type=float;;
$ac_cv_sizeof_double)      tds_sysdep_real32_type=double;;
$ac_cv_sizeof_long_double) tds_sysdep_real32_type="long double";;
*)                     AC_MSG_ERROR(No 32-bit real found.)
esac

case 8 in
$ac_cv_sizeof_float)       tds_sysdep_real64_type=float;;
$ac_cv_sizeof_double)      tds_sysdep_real64_type=double;;
$ac_cv_sizeof_long_double) tds_sysdep_real64_type="long double";;
*)                     AC_MSG_ERROR(No 64-bit real found.)
esac

case 8 in
$ac_cv_sizeof_long)      tds_sysdep_int64_type=long;;
$ac_cv_sizeof_long_long) tds_sysdep_int64_type="long long";;
$ac_cv_sizeof___int64)   tds_sysdep_int64_type=__int64;;
*)                     AC_MSG_ERROR([No 64-bit integer found.])
esac
SPRINTF_I64_FORMAT

case $ac_cv_sizeof_void_p in
$ac_cv_sizeof_short)     tds_sysdep_intptr_type=short;;
$ac_cv_sizeof_int)       tds_sysdep_intptr_type=int;;
$ac_cv_sizeof_long)      tds_sysdep_intptr_type=long;;
$ac_cv_sizeof_long_long) tds_sysdep_intptr_type="long long";;
$ac_cv_sizeof___int64)   tds_sysdep_intptr_type=__int64;;
*)                     AC_MSG_ERROR(No intptr type found.)
esac

if test "$ac_cv_sizeof_long_long" -ne 0; then
	AC_DEFINE(HAVE_LONG_LONG, 1, [Define if you have long long])
fi

AC_SUBST(tds_sysdep_int16_type)
AC_SUBST(tds_sysdep_int32_type)
AC_SUBST(tds_sysdep_real32_type)
AC_SUBST(tds_sysdep_real64_type)
AC_SUBST(tds_sysdep_int64_type)
AC_SUBST(tds_sysdep_intptr_type)

# ------------------------------------------------------------
# Checks for typedefs and structures
# ------------------------------------------------------------

# ---- Solaris needs -DBSD_COMP to get FIONBIO defined ----
AC_MSG_CHECKING(is FIONBIO defined)
AC_EGREP_CPP(yes,
[#include <sys/ioctl.h>
#ifdef FIONBIO
 yes
#endif
],
    AC_MSG_RESULT(yes),
    [ AC_EGREP_CPP(yes,
[#define BSD_COMP
#include <sys/ioctl.h>
#ifdef FIONBIO
 yes
#endif
],
    AC_DEFINE(BSD_COMP, 1, [Define to 1 if you need BSD_COMP defined to get FIONBIO defined.]) AC_MSG_RESULT(need -DBSD_COMP),
    AC_MSG_RESULT(no))
    ])

TYPE_SOCKLEN_T

AC_MSG_CHECKING([whether getopt has optreset support])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif]], [[optreset = 0;]])],
[AC_MSG_RESULT(yes)
AC_DEFINE(HAVE_GETOPT_OPTRESET, 1, [Define if your getopt(3) defines and uses optreset])],
[AC_MSG_RESULT(no)]
)


# ------------------------------------------------------------
# Checks for compiler characteristics.
# ------------------------------------------------------------
AC_C_BIGENDIAN
AC_C_CONST

case $host_os in
mingw*)
	FREETDS_SYMBOLIC="-no-undefined"
	;;
hpux* | linux* | solaris*)
AC_MSG_CHECKING([whether we can use -Wl,-Bsymbolic])
AC_LANG_CONFTEST([AC_LANG_PROGRAM([], [return 1])])
rm -f conftest.$ac_objext
CMD="$SHELL $(test -n "$ac_top_builddir" && echo $ac_top_builddir || echo .)/libtool --mode=link $CC -o libtest.la conftest.$ac_ext >/dev/null 2>&1"
if eval $CMD && test -s libtest.la; then
AC_MSG_RESULT(yes)
FREETDS_SYMBOLIC="-Wl,-Bsymbolic"
else
AC_MSG_RESULT(no)
fi
rm -f conftest.$ac_objext conftest.$ac_ext libtest.la
rm -rf .libs
;;
esac
AC_SUBST(FREETDS_SYMBOLIC)

# if GNU compiler but no GNU linker add explicitly libgcc
# this is required for Solaris for example
if test "$GCC" = yes; then
	for freetds_ldflag in "-static-libgcc" "-Wl,-static-libgcc"; do
		AC_MSG_CHECKING([whether we can use $freetds_ldflag])
		saved_LDFLAGS=$LDFLAGS
		LDFLAGS="$LDFLAGS $freetds_ldflag"
		AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],
			[ac_cv_use_freetds_flag="yes"],
			[ac_cv_use_freetds_flag="no"])
		LDFLAGS=$saved_LDFLAGS
		AC_MSG_RESULT($ac_cv_use_freetds_flag)
		if test "$ac_cv_use_freetds_flag" = "yes"; then
			FREETDS_LIBGCC="$freetds_ldflag"
			break;
		fi
	done
	AC_COMPILE_IFELSE([AC_LANG_SOURCE([static void __attribute__((destructor)) my_uninit(void) {}])],
			  AC_DEFINE(TDS_ATTRIBUTE_DESTRUCTOR, 1,
				    [Define to 1 if your compiler supports __attribute__((destructor)).]))
fi
AC_SUBST(FREETDS_LIBGCC)

TDS_NULL_IS_ZERO

# ------------------------------------------------------------
# Checks for library functions.
# ------------------------------------------------------------
AC_CHECK_FUNCS([vsnprintf _vsnprintf _vscprintf gettimeofday \
nl_langinfo locale_charset setenv putenv \
getuid getpwuid getpwuid_r fstat alarm fork \
gethrtime localtime_r setitimer eventfd \
_fseeki64 _ftelli64 setrlimit pthread_cond_timedwait \
_lock_file _unlock_file usleep nanosleep readdir_r \
mallinfo mallinfo2 _heapwalk])

AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>
#include <stdlib.h>]],
[[return system("ls") != 0;]])],
AC_DEFINE(HAVE_SYSTEM, 1, [Define to 1 if you have the 'system' function]))

ACX_PUSH_LIBS("$LIBS $NETWORK_LIBS")
AC_CHECK_FUNCS([inet_ntoa_r getipnodebyaddr getipnodebyname \
getaddrinfo inet_ntop gethostname poll socketpair strtok_s])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#include <ws2tcpip.h>
#endif]],
[[return getaddrinfo(NULL, NULL, NULL, NULL);]])],
AC_DEFINE(HAVE_GETADDRINFO, 1, [Define if you have getaddrinfo function]))
ACX_POP_LIBS

AC_REPLACE_FUNCS([asprintf vasprintf strtok_r readpassphrase \
strlcpy strlcat basename getopt strsep daemon setenv])
AC_FUNC_FSEEKO
TDS_GETTIMEMILLI
AC_FUNC_ALLOCA

# ------------------------------------------------------------
# Checks for library variables.
# ------------------------------------------------------------
AC_HAVE_MALLOC_OPTIONS

# ------------------------------------------------------------
# Compile time options
# ------------------------------------------------------------
AC_MSG_RESULT(checking compile-time options)

AC_ARG_WITH(tdsver,
AS_HELP_STRING([--with-tdsver=VERSION], [TDS protocol version (5.0/7.1/7.2/7.3/7.4/auto) [auto]]))
case "$with_tdsver" in
4.2) AC_MSG_ERROR([Protocol 4.2 cannot be specified as obsolete]) ;;
4.6) AC_MSG_ERROR([Protocol 4.6 cannot be specified as obsolete and never correctly supported]) ;;
5.0) AC_DEFINE(TDS50, 1, [Define to use TDS 5.0 by default]) ;;
7.0) AC_MSG_ERROR([Protocol 7.0 cannot be specified during configure as too insecure]) ;;
7.1) AC_DEFINE(TDS71, 1, [Define to use TDS 7.1 by default]) ;;
7.2) AC_DEFINE(TDS72, 1, [Define to use TDS 7.2 by default]) ;;
7.3) AC_DEFINE(TDS73, 1, [Define to use TDS 7.3 by default]) ;;
7.4) AC_DEFINE(TDS74, 1, [Define to use TDS 7.4 by default]) ;;
""|auto) ;;
*)   AC_MSG_ERROR([Invalid value specified for --with-tdsver: $with_tdsver]) ;;
esac

AC_ARG_WITH(iodbc,
AS_HELP_STRING([--with-iodbc=DIR], [build ODBC driver against iODBC in DIR]))

if test "x$with_iodbc" = "xyes"; then
	# The user asked for iodbc support, but didn't tell us
	# where to look. So, we'll try to get the info from
	# pkg-config. If we can't, we'll display an error.
	PKG_CHECK_MODULES(IODBC, libiodbc)
	CPPFLAGS="$CPPFLAGS -DIODBC"
	ODBC_LDFLAGS="$IODBC_LIBS"

	# The ODBC_INC variable is only substituted into *_CPPFLAGS,
	# which is there $IODBC_CFLAGS is supposed to go anyway.
	ODBC_INC="$IODBC_CFLAGS"
	odbc=true
	iodbc=true
elif test "x$with_iodbc" != "x" -a "x$with_iodbc" != "xno"; then
	if echo "$with_iodbc" | grep -v '^/'; then
		with_iodbc="$PWD/$with_iodbc"
	fi
	CPPFLAGS="$CPPFLAGS -DIODBC"
	ODBC_INC="$with_iodbc/include"
	test -r "$ODBC_INC/isql.h" || AC_MSG_ERROR([isql.h not found])
	test -r "$ODBC_INC/isqlext.h" || AC_MSG_ERROR([isqlext.h not found])
	ODBC_INC="-I$ODBC_INC"
	ODBC_LDFLAGS="-L$with_iodbc/lib -liodbc"
	odbc=true
	iodbc=true
fi

AC_ARG_WITH(unixodbc,
AS_HELP_STRING([--with-unixodbc=DIR], [build ODBC driver against unixODBC in DIR]))

if test "x$with_unixodbc" = "xyes"; then
	# The user asked for unixodbc support, but didn't tell us
	# where to look. We'll try to get the installation prefix from
	# the odbc_config executable, and if we can't, we'll display
	# an error.
	if test -z "$ODBC_CONFIG"; then
		AC_MSG_ERROR([odbc_config not found])
	fi
	with_unixodbc=`$ODBC_CONFIG --prefix`
	if ! test -x "${with_unixodbc}/bin/odbc_config"; then
		AC_MSG_ERROR([could not find your unixODBC installation])
	fi
fi

# Note: the --with-unixodbc=yes case will make use of this one,
# after populating $with_unixodbc with the correct prefix.
if test "x$with_unixodbc" != "x" -a "x$with_unixodbc" != "xno"; then
	if test "x$with_iodbc" != "x" -a "x$with_iodbc" != "xno"; then
		AC_MSG_ERROR([choose at most one of --with-iodbc or --with-unixodbc])
	fi
	if echo "$with_unixodbc" | grep -v '^/'; then
		with_unixodbc="$PWD/$with_unixodbc"
	fi
	CPPFLAGS="$CPPFLAGS -DUNIXODBC"
	if test -f "$with_unixodbc/bin/odbc_config"; then
		ODBC_CONFIG="$with_unixodbc/bin/odbc_config"
		ODBC_INC=`$ODBC_CONFIG --include-prefix`
		ODBC_LDFLAGS="-L`$ODBC_CONFIG --lib-prefix`"
	else
		# if not available do not use system default
		ODBC_CONFIG=""
		ODBC_INC="$with_unixodbc/include"
		ODBC_LDFLAGS="-L$with_unixodbc/lib"
	fi
	test -r "$ODBC_INC/sql.h" || AC_MSG_ERROR([sql.h not found])
	test -r "$ODBC_INC/sqlext.h" || AC_MSG_ERROR([sqlext.h not found])
	ODBC_INC="-I$ODBC_INC"
	ODBC_LDFLAGS="$ODBC_LDFLAGS -lodbc"
	odbc=true
	unixodbc=true
fi

AC_ARG_WITH(odbc_nodm,
AS_HELP_STRING([--with-odbc-nodm=DIR], [build ODBC using headers in DIR/include]))
if test "x$with_odbc_nodm" != "x" -a "x$with_odbc_nodm" != "xno"; then
	if echo "$with_odbc_nodm" | grep -v '^/'; then
		with_odbc_nodm="$PWD/$with_odbc_nodm"
	fi
	CPPFLAGS="$CPPFLAGS -DTDS_NO_DM"
	ODBC_INC="$with_odbc_nodm/include"
	test -r "$ODBC_INC/sql.h" || AC_MSG_ERROR([sql.h not found])
	test -r "$ODBC_INC/sqlext.h" || AC_MSG_ERROR([sqlext.h not found])
	ODBC_INC="-I$ODBC_INC"
	# The next line is for linking the unittests. Here the ODBC driver
	#  itself is treated as the driver manager to be linked.
	# Can't use ODBC_LDFLAGS/LDFLAGS variables since they are used in building
	#  other directories.
	ODBC_LDFLAGS="-L\$(top_builddir)/src/odbc -ltdsodbc"
	odbc=true
fi

build_odbc=yes
if test "$odbc" != "true"; then
AC_ARG_ENABLE(odbc,
	AS_HELP_STRING([--disable-odbc], [do not attempt to build the ODBC driver]),
	[build_odbc=$enableval], [build_odbc=yes])
fi
AM_CONDITIONAL(INCODBC, test "$build_odbc" = "yes")

if test "$build_odbc" = "yes"
then
	case $host_os in
	linux*)
		if test "$with_odbc_nodm" = ""; then
			FREETDS_ODBC_MODULE="-avoid-version"
		fi
		;;
	darwin* | rhapsody*)
		if test "$with_odbc_nodm" = ""; then
			FREETDS_ODBC_MODULE="-module"
		fi
		;;
	esac
	AC_SUBST(FREETDS_ODBC_MODULE)

	# odbc not specified, try to detect it
	echo "looking for installed ODBC driver manager"
	if test "$odbc" != "true" -a $tds_mingw = yes; then
		odbc=true
		unixodbc=true
		CPPFLAGS="$CPPFLAGS -DUNIXODBC"
		ODBC_LDFLAGS="-lodbc32"
	fi
	# try to detect searching for iODBC
	if test "$odbc" != "true" -a "x$with_iodbc" != "xno"; then
		AC_CHECK_LIB(iodbc, SQLConnect, [AC_CHECK_HEADER([isql.h], [odbc=true
	iodbc=true
	CPPFLAGS="$CPPFLAGS -DIODBC"
	ODBC_LDFLAGS="-liodbc"])])
	fi
	# try to detect searching for unixODBC
	if test "$odbc" != "true" -a "x$with_unixodbc" != "xno"; then
		AC_CHECK_LIB(odbc, SQLConnect, [AC_CHECK_HEADER([sql.h], [odbc=true
	unixodbc=true
	CPPFLAGS="$CPPFLAGS -DUNIXODBC"
	ODBC_LDFLAGS="-lodbc"])])
	fi

	if test "$odbc" = "true"; then
		saved_LDFLAGS="$LDFLAGS"
		ODBCINST_LDFLAGS=
		LDFLAGS="$LDFLAGS $ODBC_LDFLAGS"
		if test $tds_mingw = yes; then
			ODBCINST_LDFLAGS="-lodbccp32"
		elif test "$unixodbc" = "true"; then
			if test -n "$ODBC_CONFIG"; then
				CPPFLAGS="$CPPFLAGS `$ODBC_CONFIG --cflags`"
			fi
			AC_CHECK_LIB(odbcinst, SQLGetPrivateProfileString, [ODBCINST_LDFLAGS="$ODBC_LDFLAGS -lodbcinst"])
		elif test "$iodbc" = "true"; then
			AC_CHECK_LIB(iodbcinst, SQLGetPrivateProfileString, [ODBCINST_LDFLAGS="$ODBC_LDFLAGS -liodbcinst"])
		fi
		if test "$ODBCINST_LDFLAGS" != ""; then
			AC_DEFINE_UNQUOTED(HAVE_SQLGETPRIVATEPROFILESTRING, 1, [Define to 1 if you have the SQLGetPrivateProfileString function.])
		fi
		LDFLAGS="$saved_LDFLAGS"
	fi
	if test "$iodbc" = "true"; then
		save_cppflags="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $ODBC_INC"
		AC_CHECK_HEADERS(iodbcinst.h)
		CPPFLAGS="$save_cppflags"
	fi
	# test for SQLLEN (iODBC do not define but typedef it)
	save_cppflags="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $ODBC_INC"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if defined(_WIN32) || defined(WIN32) || defined(__MINGW32__)
#include <windows.h>
#endif
#include <sql.h>]], [[SQLLEN len = 123;]])],
		AC_DEFINE(HAVE_SQLLEN, 1, [Define if sqltypes.h define SQLLEN])
		# check SQLColAttribute definition
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if defined(_WIN32) || defined(WIN32) || defined(__MINGW32__)
#include <windows.h>
#endif
#include <sql.h>
#include <sqlext.h>

SQLRETURN  SQL_API SQLColAttribute (SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber, SQLUSMALLINT FieldIdentifier,
	SQLPOINTER CharacterAttribute, SQLSMALLINT BufferLength,
	SQLSMALLINT *StringLength, SQLLEN * NumericAttribute) {
}]], [[return 0]])],
		AC_DEFINE(TDS_SQLCOLATTRIBUTE_SQLLEN, 1, [Define to 1 if last argument of SQLColAttribute it's SQLLEN *]))

		# check is SQLParamOptions accept SQLULEN
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if defined(_WIN32) || defined(WIN32) || defined(__MINGW32__)
#include <windows.h>
#endif
#include <sql.h>
#include <sqlext.h>

SQLRETURN SQL_API SQLParamOptions(SQLHSTMT       hstmt,
	SQLULEN crow, SQLULEN *pirow) {
	return SQL_SUCCESS;
}]], [[return 0]])],
		AC_DEFINE(TDS_SQLPARAMOPTIONS_SQLLEN, 1, [Define to 1 if SQLParamOptions accept SQLULEN as arguments]))
	)

	AC_CHECK_TYPES([SQLSETPOSIROW, SQLROWSETSIZE, SQLROWOFFSET],,, [#if defined(_WIN32) || defined(WIN32) || defined(__MINGW32__)
#include <windows.h>
#endif
#include <sql.h>
#include <sqlext.h>])

	# test for sizeof(SQLWCHAR)
	AC_CHECK_SIZEOF(SQLWCHAR,,[#if defined(_WIN32) || defined(WIN32) || defined(__MINGW32__)
#include <windows.h>
#endif
#include <sql.h>])

	CPPFLAGS="$save_cppflags"
else
	AC_MSG_NOTICE(ODBC disabled)
fi

AM_CONDITIONAL(ODBC, test "$odbc" = "true")
AM_CONDITIONAL(UNIXODBC, test "$unixodbc" = "true")
AC_SUBST(ODBC)
AC_SUBST(ODBC_INC)
AC_SUBST(ODBC_LDFLAGS)
AC_SUBST(ODBCINST_LDFLAGS)

AC_DEFUN([TDS_CHECK_GMP], [
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>
#include <gmp.h>]],
[[mpz_t x; mpz_init(x); mpz_powm(x,x,x,x); mpz_clear(x); return 0;]])],
AC_DEFINE(HAVE_GMP, 1, [Define if you have GMP library]),
[$1])])

AC_DEFUN([TDS_HAVE_GMP],
[TDS_CHECK_GMP([gmp_save_LIBS="$LIBS"
LIBS="$LIBS -lgmp"
TDS_CHECK_GMP([LIBS="$gmp_save_LIBS"])])])

AC_ARG_WITH(gnutls,
AS_HELP_STRING([--with-gnutls], [build with GnuTLS support]))
if test "$with_gnutls" = "yes"; then
	AC_DEFINE(HAVE_GNUTLS, 1, [Define to 1 if you have GnuTLS.])
	gnutls_backend=unknown
	PKG_CHECK_MODULES(GNUTLS, gnutls,
		[CPPFLAGS="$CPPFLAGS $GNUTLS_CFLAGS"
		NETWORK_LIBS="$NETWORK_LIBS $GNUTLS_LIBS"
		if "$PKG_CONFIG" --libs --static gnutls | grep nettle > /dev/null 2>&1 ; then
			gnutls_backend=nettle
			AC_DEFINE(GNUTLS_USE_NETTLE, 1, [Define to 1 if GNU tls use nettle as backend.])
			PKG_CHECK_MODULES(NETTLE, nettle,
				[AC_DEFINE(HAVE_NETTLE, 1, [Define to 1 if nettle is present.])
				NETWORK_LIBS="$NETWORK_LIBS $NETTLE_LIBS"])
			ACX_PUSH_LIBS("$NETWORK_LIBS")
			TDS_HAVE_GMP
			AC_SEARCH_LIBS(nettle_rsa_public_key_init, [nettle hogweed])
			NETWORK_LIBS="$LIBS"
			ACX_POP_LIBS
		fi],
		[CPPFLAGS="$CPPFLAGS `libgnutls-config --cflags`"
		NETWORK_LIBS="$NETWORK_LIBS `libgnutls-config --libs`"]
	)
	ACX_PUSH_LIBS("$NETWORK_LIBS")
	AC_CHECK_FUNCS([gnutls_certificate_set_verify_function gnutls_record_disable_padding gnutls_rnd gnutls_alpn_set_protocols])
	ACX_POP_LIBS
	if test "$gnutls_backend" = "unknown"; then
		gnutls_backend=gcrypt
		NETWORK_LIBS="$NETWORK_LIBS -ltasn1"
	fi
else
	CHECK_OPENSSL
fi



AC_ARG_ENABLE(apps,
	AS_HELP_STRING([--disable-apps], [skip build of src/apps (including tsql)]))
AM_CONDITIONAL(INCAPPS, test "$enable_apps" != "no")

AC_ARG_ENABLE(server,
	AS_HELP_STRING([--disable-server], [skip build of src/server directory]))
AM_CONDITIONAL(INCSERVER, test "$enable_server" != "no")

AC_ARG_ENABLE(pool,
	AS_HELP_STRING([--disable-pool], [skip build of src/pool directory]))

AC_ARG_ENABLE(msdblib,
	AS_HELP_STRING([--enable-msdblib], [for MS style dblib]))

if test "$enable_msdblib" = "yes" ; then
	dblib_define="#define MSDBLIB 1"
else
	dblib_define="#define SYBDBLIB 1"
fi
AC_SUBST(dblib_define)

AC_ARG_ENABLE(sybase-compat,
	AS_HELP_STRING([--enable-sybase-compat], [enable increased Open Client binary compatibility]))
AM_CONDITIONAL(SYBASE_COMPAT, test x$enable_sybase_compat = xyes)
if test x$enable_sybase_compat = xyes; then
	AC_DEFINE(TDS_SYBASE_COMPAT, 1, [Defined if --enable-sybase-compat used])
fi

AC_MSG_CHECKING(threadsafety)
# Enable or disable thread safetiness
# This option is enabled by default because most OS's support it.
# A more sophisticated approach would be to set the default per-OS.
AC_ARG_ENABLE(threadsafe,
	AS_HELP_STRING([--disable-threadsafe], [disable calls to threadsafe fuctions e.g.,gethostbyname_r ]),
	[use_threadsafe=$enableval], [use_threadsafe=yes])

if test "$enable_threadsafe" != "no" ; then
	CPPFLAGS="$CPPFLAGS -D_REENTRANT -D_THREAD_SAFE"
	AC_MSG_RESULT(enabled)
else
	AC_DEFINE(TDS_NO_THREADSAFE, 1, [Define if you don't care about thread safety])
	AC_MSG_RESULT(disabled)
	enable_pool=no
fi
AM_CONDITIONAL(ENABLE_THREADSAFE, test "$enable_threadsafe" != "no")

AM_CONDITIONAL(INCPOOL, test "$enable_pool" != "no")

# newer gcc are configured to fail to resolve references to libraries not
# explicitly listed on the linker commandline.
# So to make build add explicitly gcrypt library.
if test "$enable_threadsafe" != "no" -a "$with_gnutls" = "yes" -a "$gnutls_backend" = "gcrypt"; then
	PKG_CHECK_MODULES(GCRYPT, gcrypt,
		[CPPFLAGS="$CPPFLAGS $GCRYPT_CFLAGS"
		NETWORK_LIBS="$NETWORK_LIBS $GCRYPT_LIBS"],
		[CPPFLAGS="$CPPFLAGS `libgcrypt-config --cflags`"
		NETWORK_LIBS="$NETWORK_LIBS `libgcrypt-config --libs`"])
fi

# we don't need to check netdb functions if they are already threadsafe
if test $netdb_reentrant != yes; then
	AC_caolan_FUNC_WHICH_GETHOSTBYNAME_R
	AC_raf_FUNC_WHICH_GETSERVBYNAME_R
	AC_tds_FUNC_WHICH_GETHOSTBYADDR_R
fi
AC_tds_FUNC_WHICH_GETPWUID_R
AC_tds_FUNC_WHICH_LOCALTIME_R

# easy test for pthread (no library, only mutex support)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>
#if !defined(_WIN32)
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif]],
[[pthread_mutex_lock(&mutex);
pthread_mutex_unlock(&mutex)]])], AC_DEFINE(TDS_HAVE_PTHREAD_MUTEX, 1, [Define if you have pthread with mutex support]))

# easy test for locking stdio
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>]],
[[int c;
flockfile(stdin);
c = getc_unlocked(stdin);
funlockfile(stdin);]])], AC_DEFINE(TDS_HAVE_STDIO_LOCKED, 1, [Define if stdio support locking]))

AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--disable-debug], [disable assert and other things]),
	[use_debug=$enableval], [use_debug=yes])

if test "$use_debug" = "yes" ; then
	AC_MSG_NOTICE(debug enabled)
	CPPFLAGS="$CPPFLAGS -DDEBUG=1"

	AC_ARG_ENABLE(extra-checks,
		AS_HELP_STRING([--enable-extra-checks], [enable extra checks on code]))
	if test "$enable_extra_checks" = "yes" ; then
		AC_DEFINE_UNQUOTED(ENABLE_EXTRA_CHECKS, 1, [Define to enable extra checks on code])
	fi
else
	AC_MSG_NOTICE(debug disabled)
	CPPFLAGS="$CPPFLAGS -DNDEBUG=1"
fi

AC_ARG_ENABLE(developing,
	AS_HELP_STRING([--enable-developing], [enable code still in develop]))
if test "$enable_developing" = "yes" ; then
	AC_DEFINE_UNQUOTED(ENABLE_DEVELOPING, 1, [Define to enable work in progress code])
fi
AM_CONDITIONAL(ENABLE_DEVELOPING, test "$enable_developing" = "yes")

AC_ARG_ENABLE(mars,
	AS_HELP_STRING([--disable-mars], [enable MARS code]))
if test "$enable_mars" != "no" ; then
	AC_DEFINE_UNQUOTED(ENABLE_ODBC_MARS, 1, [Define to enable MARS support])
fi

AC_ARG_ENABLE(odbc-wide,
	AS_HELP_STRING([--disable-odbc-wide], [disable wide string support in ODBC]))
if test "$enable_odbc_wide" != "no" ; then
	AC_DEFINE_UNQUOTED(ENABLE_ODBC_WIDE, 1, [Define to enable ODBC wide string support])

	AC_ARG_ENABLE(odbc-wide-tests,
		AS_HELP_STRING([--enable-odbc-wide-tests], [enable compiling ODBC tests to use wide functions]))
	AM_CONDITIONAL(ODBC_WIDE_TESTS, test "$enable_odbc_wide_tests" = "yes")
else
	AM_CONDITIONAL(ODBC_WIDE_TESTS, false)
fi

AC_ARG_ENABLE(distcheck_build,
	AS_HELP_STRING([--enable-distcheck-build], [used internally for testing]))
if test "$enable_distcheck_build" = "yes" ; then
	AM_CONDITIONAL(DISTCHECK_BUILD, true)
else
	AM_CONDITIONAL(DISTCHECK_BUILD, false)
fi

# ------------------------------------------------------------
# LTLIBOBJS hack (for autoconf-2.53)
# ------------------------------------------------------------
# This is necessary so that .o files in LIBOBJS are also built via
# the ANSI2KNR-filtering rules.
LIB@&t@OBJS=`echo "$LIB@&t@OBJS" |
             sed 's,\.[[^.]]* ,$U&,g;s,\.[[^.]]*$,$U&,'`
LTLIBOBJS=`echo "$LIB@&t@OBJS" |
           sed 's,\.[[^.]]* ,.lo ,g;s,\.[[^.]]*$,.lo,'`
AC_SUBST(LTLIBOBJS)

# enable some warning for gcc
if test "$ac_compiler_gnu" = "yes" ; then
	CPPFLAGS="$CPPFLAGS -Wall -Wstrict-prototypes -Wmissing-prototypes -Wno-long-long -Wpointer-arith -Wunused-parameter"
fi

AX_CFLAGS_GCC_OPTION([-Wdeclaration-after-statement])

AC_MSG_RESULT(done)
# ------------------------------------------------------------
# Final output
# ------------------------------------------------------------
AC_CONFIG_FILES(include/freetds/version.h \
	include/tds_sysdep_public.h \
	include/freetds/sysdep_types.h \
	Makefile \
	include/Makefile \
	include/freetds/Makefile \
	doc/Makefile \
	samples/Makefile \
	src/Makefile \
	src/tds/Makefile src/tds/unittests/Makefile \
	src/dblib/Makefile src/dblib/unittests/Makefile \
	src/ctlib/Makefile src/ctlib/unittests/Makefile \
	src/replacements/Makefile \
	src/replacements/unittests/Makefile \
	src/utils/Makefile \
	src/utils/unittests/Makefile \
	src/server/Makefile \
	src/pool/Makefile \
	src/odbc/Makefile \
	src/odbc/unittests/Makefile \
	src/apps/Makefile \
	src/apps/fisql/Makefile \
	src/apps/unittests/Makefile \
	freetds.spec \
	win32/Makefile \
	win32/freetds.nsh \
	src/odbc/version.rc \
	vms/Makefile misc/Makefile \
	doc/freebcp.1 doc/tsql.1 doc/osql.1 \
	doc/bsqldb.1 doc/bsqlodbc.1 doc/defncopy.1 \
	doc/datacopy.1 doc/fisql.1 doc/freetds.conf.5
)
AC_OUTPUT
