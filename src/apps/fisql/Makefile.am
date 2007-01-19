# $Id: Makefile.am,v 1.2 2007-01-19 18:49:55 castellano Exp $

AM_CPPFLAGS	= -I$(top_srcdir)/include 

bin_PROGRAMS	= fisql

fisql_LDADD	= ../../dblib/libsybdb.la ../../replacements/libreplacements.la $(NETWORK_LIBS) $(READLINE_LIBS)
fisql_SOURCES	= fisql.c terminal.c terminal.h edit.c edit.h handlers.c handlers.h interrupt.c interrupt.h

