NULL =
TESTS =	\
	strings$(EXEEXT) \
	strtok_r$(EXEEXT) \
	strsep$(EXEEXT) \
	$(NULL)
check_PROGRAMS = $(TESTS)

strings_SOURCES = strings.c
strtok_r_SOURCES = strtok_r.c
strsep_SOURCES = strsep.c

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/.. -I../
LDADD = ../../utils/unittests/libtds_test_base.a \
	../../replacements/libreplacements.la $(NETWORK_LIBS)
EXTRA_DIST = CMakeLists.txt

