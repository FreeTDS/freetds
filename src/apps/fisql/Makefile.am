# $Id: Makefile.am,v 1.3.2.1 2010-07-28 12:35:50 freddy77 Exp $

AM_CPPFLAGS	= -I$(top_srcdir)/include 

if MINGW32
else
bin_PROGRAMS	= fisql

fisql_LDADD	= ../../dblib/libsybdb.la ../../replacements/libreplacements.la $(NETWORK_LIBS) $(LIBICONV) $(READLINE_LIBS)
fisql_SOURCES	= fisql.c terminal.c terminal.h edit.c edit.h handlers.c handlers.h interrupt.c interrupt.h
endif
