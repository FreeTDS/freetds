# $Id: Makefile.am,v 1.12 2005-05-31 09:31:57 freddy77 Exp $

bin_PROGRAMS	= tsql freebcp bsqldb defncopy datacopy
INCLUDES	= -I$(top_srcdir)/include 
freebcp_LDADD	= ../dblib/libsybdb.la $(NETWORK_LIBS)

freebcp_SOURCES = freebcp.c freebcp.h

tsql_LDADD	= ../tds/libtds_objects.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC) $(READLINE_LIBS)

bsqldb_LDADD	=    ../dblib/libsybdb.la $(NETWORK_LIBS)
defncopy_LDADD	=    ../dblib/libsybdb.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS)

datacopy_SOURCES = datacopy.c
datacopy_LDADD  =    ../dblib/libsybdb.la $(NETWORK_LIBS)
