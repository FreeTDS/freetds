# $Id: Makefile.am,v 1.8.2.1 2004-12-08 11:03:08 freddy77 Exp $
bin_PROGRAMS   =    tsql freebcp bsqldb defncopy
INCLUDES  =    -I$(top_srcdir)/include 
freebcp_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)

freebcp_SOURCES = freebcp.c freebcp.h
tsql_LDADD	= ../replacements/libreplacements.la ../tds/libtds_objects.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC) $(READLINE_LIBS)

bsqldb_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)
defncopy_LDADD   =    ../dblib/libsybdb.la $(NETWORK_LIBS)

