# $Id: Makefile.am,v 1.8 2004-04-12 20:36:42 jklowden Exp $
bin_PROGRAMS   =    tsql freebcp bsqldb defncopy
INCLUDES  =    -I$(top_srcdir)/include 
LDADD     =    ../tds/libtds.la
freebcp_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)

freebcp_SOURCES = freebcp.c freebcp.h
tsql_LDADD	= $(READLINE_LIBS) $(LDADD) $(NETWORK_LIBS)

bsqldb_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)
defncopy_LDADD   =    ../dblib/libsybdb.la $(NETWORK_LIBS)

