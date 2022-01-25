NULL =
TESTS = \
	passarg$(EXEEXT) \
	condition$(EXEEXT) \
	mutex1$(EXEEXT) \
	dlist$(EXEEXT) \
	bytes$(EXEEXT) \
	smp$(EXEEXT) \
	$(NULL)
check_PROGRAMS = $(TESTS)

passarg_SOURCES = passarg.c
condition_SOURCES = condition.c
mutex1_SOURCES = mutex1.c
dlist_SOURCES = dlist.c
smp_SOURCES = smp.c
if !HAVE_SSPI
TESTS += challenge$(EXEEXT)
challenge_SOURCES= challenge.c
endif

AM_CPPFLAGS = -I$(top_srcdir)/include
LDADD = ../libtdsutils.la @NETWORK_LIBS@
EXTRA_DIST = CMakeLists.txt challenge.c
