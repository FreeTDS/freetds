bin_PROGRAMS   =    tsql freebcp
INCLUDES  =    -I$(top_srcdir)/include 
LDADD     =    ../tds/libtds.la
freebcp_LDADD     =    ../dblib/libsybdb.la

freebcp_SOURCES = freebcp.c freebcp.h
tsql_LDADD	= $(READLINE_LIBS) $(LDADD)
