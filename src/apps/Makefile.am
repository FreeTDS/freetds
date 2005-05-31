# $Id: Makefile.am,v 1.11 2005-05-31 07:26:05 freddy77 Exp $

bin_PROGRAMS	= tsql freebcp bsqldb defncopy tdsping
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

tdsping_SOURCES	= tdsping.c
tdsping_LDADD	= ../tds/libtds_objects.la ../replacements/libreplacements.la \
		   $(NETWORK_LIBS) $(FREETDS_LIBGCC)

