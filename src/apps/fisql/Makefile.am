# $Id: Makefile.am,v 1.1 2007-01-16 21:33:12 castellano Exp $

AM_CPPFLAGS	= -I$(top_srcdir)/include 

bin_PROGRAMS	= fisql

fisql_LDADD	= ../../dblib/libsybdb.la ../../replacements/libreplacements.la $(NETWORK_LIBS) $(READLINE_LIBS)
fisql_SOURCES	= fisql.c terminal.c terminal.h edit.c edit.h handlers.c handlers.h interrupt.c interrupt.h xgetpass.c xgetpass.h

