# $Id: Makefile.am,v 1.13 2006-06-14 15:30:13 freddy77 Exp $
AM_CPPFLAGS=			-I$(top_srcdir)/include
noinst_LTLIBRARIES=		libreplacements.la
libreplacements_la_SOURCES=	iconv.c gettimeofday.c
libreplacements_la_LDFLAGS=
libreplacements_la_LIBADD=	@LTLIBOBJS@
EXTRA_DIST=			asprintf.c vasprintf.c atoll.c strtok_r.c \
	readpassphrase.c basename.c strlcpy.c strlcat.c
