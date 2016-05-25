TESTS =
check_PROGRAMS = $(TESTS)

if !HAVE_SSPI
TESTS += challenge$(EXEEXT)
challenge_SOURCES= challenge.c
endif

AM_CPPFLAGS = -I$(top_srcdir)/include
LIBS = ../libtdsutils.la @NETWORK_LIBS@
EXTRA_DIST = CMakeLists.txt challenge.c
