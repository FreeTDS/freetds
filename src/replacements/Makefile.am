# $Id: Makefile.am,v 1.20 2012-03-06 20:40:12 freddy77 Exp $
SUBDIRS=	. unittests
AM_CPPFLAGS=			-I$(top_srcdir)/include -I$(top_srcdir)/src/replacements
noinst_LTLIBRARIES=		libreplacements.la
libreplacements_la_SOURCES =	\
	iconv.c gettimeofday.c poll.c getpassarg.c socketpair.c win_mutex.c \
	tds_cond.c getaddrinfo.c sleep.c

libreplacements_la_LDFLAGS=
libreplacements_la_LIBADD=	@LTLIBOBJS@
EXTRA_DIST=	asprintf.c \
		basename.c \
		getopt.c \
		readpassphrase.c \
		strlcat.c \
		getaddrinfo.c \
		strlcpy.c \
		strtok_r.c \
		strsep.c \
		vasprintf.c \
		socketpair.c \
		ptw32_MCS_lock.c \
		daemon.c \
		CMakeLists.txt

