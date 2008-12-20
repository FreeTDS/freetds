# $Id: Makefile.am,v 1.14 2008-12-20 06:01:47 jklowden Exp $
AM_CPPFLAGS=			-I$(top_srcdir)/include -I$(top_srcdir)/src/replacements
noinst_LTLIBRARIES=		libreplacements.la
libreplacements_la_SOURCES=	iconv.c gettimeofday.c fakepoll.c fakepoll.h
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

