# $Id: Makefile.am,v 1.9 2004-12-07 17:51:17 freddy77 Exp $
bin_PROGRAMS   =    tsql freebcp bsqldb defncopy
INCLUDES  =    -I$(top_srcdir)/include 
LDADD     =    ../tds/libtds.la
freebcp_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)

freebcp_SOURCES = freebcp.c freebcp.h
tsql_LDADD	= $(LDADD) $(READLINE_LIBS) $(NETWORK_LIBS)

bsqldb_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)
defncopy_LDADD   =    ../dblib/libsybdb.la $(NETWORK_LIBS)

