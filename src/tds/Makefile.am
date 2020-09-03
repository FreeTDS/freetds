NULL=
SUBDIRS			=	. unittests
AM_CPPFLAGS		=	-I$(top_srcdir)/include

noinst_LTLIBRARIES	=	libtds.la
libtds_la_SOURCES = \
	mem.c \
	token.c \
	util.c \
	login.c \
	read.c \
	write.c \
	convert.c \
	numeric.c \
	config.c \
	query.c \
	iconv.c \
	locale.c \
	vstrbuild.c \
	getmac.c \
	data.c \
	net.c \
	tls.c \
	tds_checks.c \
	log.c \
	bulk.c \
	packet.c \
	stream.c \
	random.c \
	sec_negotiate.c \
	sec_negotiate_gnutls.h \
	sec_negotiate_openssl.h \
	gssapi.c \
	$(NULL)
if HAVE_SSPI
libtds_la_SOURCES += sspi.c
else
libtds_la_SOURCES += challenge.c
endif
libtds_la_LDFLAGS =
libtds_la_LIBADD = $(NETWORK_LIBS)

GENERATED_HEADER_FILES = tds_willconvert.h num_limits.h tds_types.h
noinst_HEADERS = $(GENERATED_HEADER_FILES)
EXTRA_DIST = $(GENERATED_HEADER_FILES) \
	CMakeLists.txt \
	challenge.c \
	sspi.c \
	$(NULL)

# Perl is needed to build from a repository checkout.
# Perl is *not* needed to build from a tarball.  

## We have to build num_limits.h because automake can't infer the
## dependency (on num_limits.pl).  

data.c:	tds_types.h

if HAVE_PERL_SOURCES
BUILT_SOURCES = $(GENERATED_HEADER_FILES)

clean-local: 
	cd $(srcdir) && rm -f $(BUILT_SOURCES)

tds_willconvert.h: tds_willconvert.pl Makefile
	perl $(srcdir)/tds_willconvert.pl $(top_srcdir)/include/freetds/proto.h > $@.tmp
	mv $@.tmp $@

num_limits.h: num_limits.pl Makefile
	perl $(srcdir)/num_limits.pl > $@.tmp
	mv $@.tmp $@

tds_types.h: types.pl Makefile $(top_srcdir)/misc/types.csv
	perl $(srcdir)/types.pl $(top_srcdir)/misc/types.csv $(top_srcdir)/include/freetds/proto.h > $@.tmp
	mv $@.tmp $@
endif
