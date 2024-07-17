NULL =
TESTS = \
	passarg$(EXEEXT) \
	condition$(EXEEXT) \
	mutex1$(EXEEXT) \
	dlist$(EXEEXT) \
	bytes$(EXEEXT) \
	smp$(EXEEXT) \
	path$(EXEEXT) \
	$(NULL)
check_PROGRAMS = $(TESTS)

passarg_SOURCES = passarg.c
condition_SOURCES = condition.c
mutex1_SOURCES = mutex1.c
dlist_SOURCES = dlist.c
smp_SOURCES = smp.c
path_SOURCES = path.c
if !HAVE_SSPI
TESTS += challenge$(EXEEXT)
challenge_SOURCES= challenge.c
endif

noinst_LIBRARIES = libtds_test_base.a
libtds_test_base_a_SOURCES = \
	test_base.c \
	$(NULL)

AM_CPPFLAGS = -I$(top_srcdir)/include
LDADD = libtds_test_base.a ../libtdsutils.la @NETWORK_LIBS@
EXTRA_DIST = CMakeLists.txt challenge.c
