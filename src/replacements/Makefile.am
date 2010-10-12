# $Id: Makefile.am,v 1.17 2010-10-12 15:36:24 jklowden Exp $
AM_CPPFLAGS=			-I$(top_srcdir)/include -I$(top_srcdir)/src/replacements
noinst_LTLIBRARIES=		libreplacements.la
libreplacements_la_SOURCES=	iconv.c gettimeofday.c fakepoll.c
libreplacements_la_LDFLAGS=
libreplacements_la_LIBADD=	@LTLIBOBJS@
EXTRA_DIST=	asprintf.c \
		atoll.c \
		basename.c \
		getopt.c \
		readpassphrase.c \
		strlcat.c \
		strlcpy.c \
		strtok_r.c \
		vasprintf.c \
		replacements.vcproj

