# $Id: Makefile.am,v 1.6 2004-02-03 17:48:27 jklowden Exp $
bin_PROGRAMS   =    tsql freebcp
INCLUDES  =    -I$(top_srcdir)/include 
LDADD     =    ../tds/libtds.la
freebcp_LDADD     =    ../dblib/libsybdb.la $(NETWORK_LIBS)

freebcp_SOURCES = freebcp.c freebcp.h
tsql_LDADD	= $(READLINE_LIBS) $(LDADD) $(NETWORK_LIBS)
