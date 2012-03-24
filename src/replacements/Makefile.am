# $Id: Makefile.am,v 1.20 2012-03-06 20:40:12 freddy77 Exp $
AM_CPPFLAGS=			-I$(top_srcdir)/include -I$(top_srcdir)/src/replacements
noinst_LTLIBRARIES=		libreplacements.la
libreplacements_la_SOURCES=	iconv.c gettimeofday.c fakepoll.c getpassarg.c socketpair.c win_mutex.c
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
		socketpair.c \
		replacements.vcproj

