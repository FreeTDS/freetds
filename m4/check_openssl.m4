dnl $Id: check_openssl.m4,v 1.2 2006-03-27 07:22:54 jklowden Exp $
# OpenSSL check

AC_DEFUN([CHECK_OPENSSL],
[AC_MSG_CHECKING(if openssl is wanted)
AC_ARG_WITH(openssl, AS_HELP_STRING([--with-openssl], [--with-openssl=DIR build with OpenSSL (license NOT compatible cf. User Guide)]))
if test "$with_openssl" != "no"; then
    AC_MSG_RESULT(yes)
    old_NETWORK_LIBS="$NETWORK_LIBS"
    PKG_CHECK_MODULES(OPENSSL, [openssl], [found_ssl=yes
CFLAGS="$CFLAGS $OPENSSL_CFLAGS"
NETWORK_LIBS="$NETWORK_LIBS $OPENSSL_LIBS"], [found_ssl=no
    if test "$cross_compiling" != "yes"; then
        for dir in $withval /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/local /usr; do
            ssldir="$dir"
            if test -f "$dir/include/openssl/ssl.h"; then
                echo "OpenSSL found in $ssldir"
                found_ssl="yes"
                CFLAGS="$CFLAGS -I$ssldir/include"
                NETWORK_LIBS="$NETWORK_LIBS -lssl -lcrypto"
                LDFLAGS="$LDFLAGS -L$ssldir/lib"
                break
            fi
        done
    fi])
    if test x$found_ssl = xyes; then
	ACX_PUSH_LIBS("$NETWORK_LIBS")
        AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <openssl/ssl.h>]], [[SSL_read(NULL, NULL, 100);]])], [], [found_ssl=no])
	ACX_POP_LIBS
    fi
    if test x$found_ssl != xyes -a "$with_openssl" != ""; then
        AC_MSG_ERROR(Cannot find OpenSSL libraries)
        NETWORK_LIBS="$old_NETWORK_LIBS"
    elif test x$found_ssl = xyes; then
        HAVE_OPENSSL=yes
        ACX_PUSH_LIBS("$NETWORK_LIBS")
        AC_CHECK_FUNCS([BIO_get_data RSA_get0_key ASN1_STRING_get0_data SSL_set_alpn_protos])
        ACX_POP_LIBS
        AC_DEFINE(HAVE_OPENSSL, 1, [Define if you have the OpenSSL.])
    else
        NETWORK_LIBS="$old_NETWORK_LIBS"
    fi
    AC_SUBST(HAVE_OPENSSL)
else
    AC_MSG_RESULT(no)
fi
])
