# $Id: Makefile.am,v 1.12 2006-03-27 07:22:54 jklowden Exp $
AM_CPPFLAGS=			-I$(top_srcdir)/include
noinst_LTLIBRARIES=		libreplacements.la
libreplacements_la_SOURCES=	iconv.c
libreplacements_la_LDFLAGS=
libreplacements_la_LIBADD=	@LTLIBOBJS@
EXTRA_DIST=			asprintf.c vasprintf.c atoll.c strtok_r.c \
	readpassphrase.c basename.c strlcpy.c strlcat.c
