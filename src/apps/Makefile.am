# $Id: Makefile.am,v 1.7 2004-04-05 07:12:09 jklowden Exp $
bin_PROGRAMS   =    tsql freebcp bsqldb
INCLUDES  =    -I$(top_srcdir)/include 
LDADD     =    ../tds/libtds.la
freebcp_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)

freebcp_SOURCES = freebcp.c freebcp.h
tsql_LDADD	= $(READLINE_LIBS) $(LDADD) $(NETWORK_LIBS)

bsqldb_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)

