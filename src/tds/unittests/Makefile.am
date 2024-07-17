NULL=
TESTS =	\
	t0001$(EXEEXT) \
	t0002$(EXEEXT) \
	t0003$(EXEEXT) \
	t0004$(EXEEXT) \
	t0005$(EXEEXT) \
	t0006$(EXEEXT) \
	t0007$(EXEEXT) \
	t0008$(EXEEXT) \
	dynamic1$(EXEEXT) \
	convert$(EXEEXT) \
	dataread$(EXEEXT) \
	utf8_1$(EXEEXT) \
	utf8_2$(EXEEXT) \
	utf8_3$(EXEEXT) \
	numeric$(EXEEXT) \
	iconv_fread$(EXEEXT) \
	toodynamic$(EXEEXT) \
	readconf$(EXEEXT) \
	charconv$(EXEEXT) \
	nulls$(EXEEXT) \
	corrupt$(EXEEXT) \
	declarations$(EXEEXT) \
	portconf$(EXEEXT) \
	parsing$(EXEEXT) \
	freeze$(EXEEXT) \
	strftime$(EXEEXT) \
	log_elision$(EXEEXT) \
	convert_bounds$(EXEEXT) \
	tls$(EXEEXT) \
	$(NULL)

# flags test commented, not necessary for 0.62
# TODO add flags test again when needed

check_PROGRAMS	=	$(TESTS)

t0001_SOURCES	=	t0001.c
t0002_SOURCES	=	t0002.c
t0003_SOURCES	=	t0003.c
t0004_SOURCES	=	t0004.c
t0005_SOURCES	=	t0005.c
t0006_SOURCES	=	t0006.c
t0007_SOURCES	=	t0007.c
t0008_SOURCES	=	t0008.c
dynamic1_SOURCES =	dynamic1.c
convert_SOURCES	=	convert.c
dataread_SOURCES	=	dataread.c
# flags_SOURCES	=	flags.c
utf8_1_SOURCES	=	utf8_1.c
utf8_2_SOURCES	=	utf8_2.c
utf8_3_SOURCES	=	utf8_3.c
if ENABLE_DEVELOPING
noinst_PROGRAMS	=	collations
collations_SOURCES	=	collations.c
endif
numeric_SOURCES =       numeric.c
iconv_fread_SOURCES	= iconv_fread.c
charconv_SOURCES	= charconv.c
toodynamic_SOURCES	= toodynamic.c
nulls_SOURCES	= nulls.c
readconf_SOURCES	= readconf.c readconf.in
corrupt_SOURCES	=	corrupt.c
declarations_SOURCES	=	declarations.c
portconf_SOURCES	= portconf.c portconf.in
parsing_SOURCES	=	parsing.c
freeze_SOURCES	=	freeze.c
strftime_SOURCES	=	strftime.c
log_elision_SOURCES	=	log_elision.c
convert_bounds_SOURCES	=	convert_bounds.c
tls_SOURCES	=	tls.c

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = common.c common.h utf8.c allcolumns.c

AM_CPPFLAGS	=	-I$(top_srcdir)/include -I$(srcdir)/.. -I../ -DFREETDS_TOPDIR=\"$(top_srcdir)\"
if FAST_INSTALL
AM_LDFLAGS	=	-no-fast-install
else
AM_LDFLAGS	=	-no-install -L../.libs -R "$(abs_builddir)/../.libs"
endif
LDADD		=	libcommon.a ../../utils/unittests/libtds_test_base.a \
			../libtds.la ../../replacements/libreplacements.la \
			$(LTLIBICONV) $(NETWORK_LIBS)
CLEANFILES	=	tdsdump.out
EXTRA_DIST	=	CMakeLists.txt

