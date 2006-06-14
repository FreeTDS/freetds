# $Id: Makefile.am,v 1.14 2006-06-14 11:26:42 freddy77 Exp $

AM_CPPFLAGS	= -I$(top_srcdir)/include 
bin_PROGRAMS	= tsql freebcp bsqldb defncopy datacopy

freebcp_LDADD	= ../dblib/libsybdb.la $(NETWORK_LIBS)

freebcp_SOURCES = freebcp.c freebcp.h

tsql_LDADD	= ../tds/libtds_objects.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC) $(READLINE_LIBS)

bsqldb_LDADD	= ../dblib/libsybdb.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS)
defncopy_LDADD	= ../dblib/libsybdb.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS)

datacopy_SOURCES= datacopy.c
datacopy_LDADD	= ../dblib/libsybdb.la $(NETWORK_LIBS)
