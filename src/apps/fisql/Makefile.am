# $Id: Makefile.am,v 1.4 2009-01-09 04:59:58 jklowden Exp $

AM_CPPFLAGS	= -I$(top_srcdir)/include 

if MINGW32
else
bin_PROGRAMS	= fisql

fisql_SOURCES	= fisql.c  \
		terminal.h terminal.c  \
		edit.h edit.c  \
		handlers.h handlers.c  \
		interrupt.h interrupt.c
		

fisql_LDADD	= ../../dblib/libsybdb.la \
		  ../../replacements/libreplacements.la \
		  $(NETWORK_LIBS) $(LIBICONV) $(READLINE_LIBS)
endif
