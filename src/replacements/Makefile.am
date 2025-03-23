NULL =
SUBDIRS = . unittests
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src/replacements
noinst_LTLIBRARIES = libreplacements.la
libreplacements_la_SOURCES = \
	iconv.c \
	$(NULL)
if WIN32
libreplacements_la_SOURCES += gettimeofday.c
endif

libreplacements_la_LDFLAGS =
libreplacements_la_LIBADD = @LTLIBOBJS@ ../utils/libtdsutils.la
EXTRA_DIST = \
	asprintf.c \
	basename.c \
	getopt.c \
	readpassphrase.c \
	strlcat.c \
	getaddrinfo.c \
	poll.c \
	strlcpy.c \
	strtok_r.c \
	strsep.c \
	vasprintf.c \
	socketpair.c \
	daemon.c \
	setenv.c \
	CMakeLists.txt \
	$(PERL_OUTPUT) \
	$(NULL)

PERL_OUTPUT = iconv_charsets.h

if HAVE_PERL_SOURCES
BUILT_SOURCES = $(PERL_OUTPUT)

clean-local:
	cd $(srcdir) && rm -f $(BUILT_SOURCES)

iconv_charsets.h: iconv_charsets.pl Makefile
	perl $(srcdir)/iconv_charsets.pl > $@.tmp
	mv $@.tmp $@
endif
