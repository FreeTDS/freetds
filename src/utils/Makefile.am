NULL =
SUBDIRS = . unittests
AM_CPPFLAGS = -I$(top_srcdir)/include
noinst_LTLIBRARIES = libtdsutils.la
libtdsutils_la_SOURCES = \
	md4.c \
	md5.c \
	des.c \
	hmac_md5.c \
	dlist.c \
	getpassarg.c \
	sleep.c \
	tds_cond.c \
	threadsafe.c \
	bjoern-utf8.c \
	tdsstring.c \
	strndup.c \
	net.c \
	smp.c \
	path.c \
	$(NULL)
if WIN32
libtdsutils_la_SOURCES += win_mutex.c
endif
EXTRA_DIST = \
	CMakeLists.txt \
	ptw32_MCS_lock.c \
	$(NULL)
