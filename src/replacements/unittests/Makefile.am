NULL =
TESTS =	\
	mutex1$(EXEEXT) \
	strings$(EXEEXT) \
	strtok_r$(EXEEXT) \
	$(NULL)
check_PROGRAMS = $(TESTS)

mutex1_SOURCES = mutex1.c
strings_SOURCES = strings.c
strtok_r_SOURCES = strtok_r.c

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/.. -I../
LIBS = ../../replacements/libreplacements.la $(NETWORK_LIBS)
EXTRA_DIST = CMakeLists.txt

