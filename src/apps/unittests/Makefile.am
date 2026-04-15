NULL=
TESTS =	\
	defncopy$(EXEEXT) \
	$(NULL)

check_PROGRAMS	=	$(TESTS)

defncopy_SOURCES	=	defncopy.c

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = common.c common.h

AM_CPPFLAGS	=	-I$(top_srcdir)/include -I$(srcdir)/.. -I../ -DFREETDS_TOPDIR=\"$(top_srcdir)\"
LDADD		=	libcommon.a \
			../../utils/unittests/libtds_test_base.a \
			../../replacements/libreplacements.la $(LTLIBICONV) \
			$(NETWORK_LIBS)
EXTRA_DIST	=	CMakeLists.txt

