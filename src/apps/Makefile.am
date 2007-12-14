# $Id: Makefile.am,v 1.24 2007-12-14 04:47:53 jklowden Exp $

AM_CPPFLAGS	= -I$(top_srcdir)/include 

SUBDIRS		= fisql

DIST_SUBDIRS	= $(SUBDIRS)

bin_PROGRAMS	= tsql freebcp bsqldb defncopy datacopy

# build bsqlodbc only if the ODBC library was to be built
if ODBC
  bin_PROGRAMS	+= bsqlodbc 
endif

dist_bin_SCRIPTS = osql

freebcp_LDADD	= ../dblib/libsybdb.la $(NETWORK_LIBS)
freebcp_SOURCES = freebcp.c freebcp.h

tsql_LDADD	= ../tds/libtds.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC) $(READLINE_LIBS)

bsqldb_LDADD	= ../dblib/libsybdb.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS)

if ODBC
bsqlodbc_LDADD	= ../odbc/libtdsodbc.la \
		  ../replacements/libreplacements.la \
		  $(ODBCLIB) $(NETWORK_LIBS)
bsqlodbc_CPPFLAGS	= $(ODBC_INC) $(AM_CPPFLAGS)
endif

defncopy_LDADD	= ../dblib/libsybdb.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS)

datacopy_SOURCES= datacopy.c
datacopy_LDADD	= ../dblib/libsybdb.la ../replacements/libreplacements.la $(NETWORK_LIBS)
