# $Id: Makefile.am,v 1.8.2.3 2004-12-09 13:03:12 jklowden Exp $

bin_PROGRAMS	= tsql freebcp bsqldb defncopy
INCLUDES	= -I$(top_srcdir)/include 
freebcp_LDADD	= ../dblib/libsybdb.la $(NETWORK_LIBS)

freebcp_SOURCES = freebcp.c freebcp.h

tsql_LDADD	= ../tds/libtds_objects.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC) $(READLINE_LIBS)

bsqldb_LDADD	=    ../dblib/libsybdb.la $(NETWORK_LIBS)
defncopy_LDADD	=    ../dblib/libsybdb.la $(NETWORK_LIBS)

