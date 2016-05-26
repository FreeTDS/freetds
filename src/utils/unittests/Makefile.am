NULL =
TESTS = \
	passarg$(EXEEXT) \
	condition$(EXEEXT) \
	$(NULL)
check_PROGRAMS = $(TESTS)

passarg_SOURCES = passarg.c
condition_SOURCES = condition.c
if !HAVE_SSPI
TESTS += challenge$(EXEEXT)
challenge_SOURCES= challenge.c
endif

AM_CPPFLAGS = -I$(top_srcdir)/include
LIBS = ../libtdsutils.la @NETWORK_LIBS@
EXTRA_DIST = CMakeLists.txt challenge.c
