TESTS		=	mutex1$(EXEEXT) condition$(EXEEXT) strings$(EXEEXT) passarg$(EXEEXT) \
			strtok_r$(EXEEXT)
check_PROGRAMS	=	$(TESTS)

mutex1_SOURCES	=	mutex1.c
condition_SOURCES	=	condition.c
strings_SOURCES	=	strings.c
passarg_SOURCES	=	passarg.c
strtok_r_SOURCES	=	strtok_r.c

AM_CPPFLAGS	=	-I$(top_srcdir)/include -I$(srcdir)/.. -I../
LIBS		=	../../replacements/libreplacements.la $(NETWORK_LIBS)
EXTRA_DIST	=	CMakeLists.txt

