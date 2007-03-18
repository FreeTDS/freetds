# $Id: Makefile.am,v 1.21 2007-03-18 11:07:30 freddy77 Exp $

AM_CPPFLAGS	= -I$(top_srcdir)/include 

SUBDIRS		= fisql

DIST_SUBDIRS	= $(SUBDIRS)

bin_PROGRAMS	= tsql freebcp bsqldb bsqlodbc defncopy datacopy

dist_bin_SCRIPTS = osql

freebcp_LDADD	= ../dblib/libsybdb.la $(NETWORK_LIBS)
freebcp_SOURCES = freebcp.c freebcp.h

tsql_LDADD	= ../tds/libtds.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC) $(READLINE_LIBS)

bsqldb_LDADD	= ../dblib/libsybdb.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS)
bsqlodbc_LDADD	= ../odbc/libtdsodbc.la \
		  ../replacements/libreplacements.la \
		  $(ODBCLIB) $(NETWORK_LIBS)
defncopy_LDADD	= ../dblib/libsybdb.la \
		  ../replacements/libreplacements.la \
		  $(NETWORK_LIBS)

datacopy_SOURCES= datacopy.c
datacopy_LDADD	= ../dblib/libsybdb.la ../replacements/libreplacements.la $(NETWORK_LIBS)
