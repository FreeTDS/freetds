# $Id: Makefile.am,v 1.15 2009-01-07 02:58:37 jklowden Exp $
AM_CPPFLAGS=			-I$(top_srcdir)/include -I$(top_srcdir)/src/replacements
noinst_LTLIBRARIES=		libreplacements.la
libreplacements_la_SOURCES=	iconv.c gettimeofday.c fakepoll.c
libreplacements_la_LDFLAGS=
libreplacements_la_LIBADD=	@LTLIBOBJS@
EXTRA_DIST=	asprintf.c \
		atoll.c \
		basename.c \
		readpassphrase.c \
		strlcat.c \
		strlcpy.c \
		strtok_r.c \
		vasprintf.c 

