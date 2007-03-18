# $Id: Makefile.am,v 1.3 2007-03-18 11:07:30 freddy77 Exp $

AM_CPPFLAGS	= -I$(top_srcdir)/include 

if MINGW32
else
bin_PROGRAMS	= fisql

fisql_LDADD	= ../../dblib/libsybdb.la ../../replacements/libreplacements.la $(NETWORK_LIBS) $(READLINE_LIBS)
fisql_SOURCES	= fisql.c terminal.c terminal.h edit.c edit.h handlers.c handlers.h interrupt.c interrupt.h
endif
