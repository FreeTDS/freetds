# $Id: Makefile.am,v 1.5.2.1 2004-04-09 22:44:38 jklowden Exp $
bin_PROGRAMS   =    tsql freebcp bsqldb
INCLUDES  =    -I$(top_srcdir)/include 
LDADD     =    ../tds/libtds.la
freebcp_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)

freebcp_SOURCES = freebcp.c freebcp.h
tsql_LDADD	= $(READLINE_LIBS) $(LDADD) $(NETWORK_LIBS)

bsqldb_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)

