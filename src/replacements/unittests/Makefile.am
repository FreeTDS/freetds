TESTS		=	mutex1$(EXEEXT) 
check_PROGRAMS	=	$(TESTS)

mutex1_SOURCES	=	mutex1.c

AM_CPPFLAGS	=	-I$(top_srcdir)/include -I$(srcdir)/.. -I../
LIBS		=	../../replacements/libreplacements.la @NETWORK_LIBS@
#EXTRA_DIST	=	challenge.c

