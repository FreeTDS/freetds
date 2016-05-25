NULL =
SUBDIRS = . unittests
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src/replacements
noinst_LTLIBRARIES = libreplacements.la
libreplacements_la_SOURCES = \
	iconv.c \
	gettimeofday.c \
	poll.c \
	getpassarg.c \
	socketpair.c \
	win_mutex.c \
	tds_cond.c \
	getaddrinfo.c \
	sleep.c \
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
	ptw32_MCS_lock.c \
	daemon.c \
	CMakeLists.txt \
	$(NULL)

