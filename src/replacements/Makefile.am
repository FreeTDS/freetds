NULL =
SUBDIRS = . unittests
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src/replacements
noinst_LTLIBRARIES = libreplacements.la
libreplacements_la_SOURCES = \
	iconv.c \
	gettimeofday.c \
	poll.c \
	socketpair.c \
	getaddrinfo.c \
	$(NULL)

libreplacements_la_LDFLAGS =
libreplacements_la_LIBADD = @LTLIBOBJS@ ../utils/libtdsutils.la
EXTRA_DIST = \
	asprintf.c \
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
	daemon.c \
	CMakeLists.txt \
	$(NULL)

