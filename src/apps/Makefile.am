# $Id: Makefile.am,v 1.31 2011-06-08 09:25:51 freddy77 Exp $

AM_CPPFLAGS	= -I$(top_srcdir)/include 

SUBDIRS		= fisql

DIST_SUBDIRS	= $(SUBDIRS)

bin_PROGRAMS	= tsql freebcp bsqldb defncopy datacopy
# build bsqlodbc only if the ODBC library was to be built
if ODBC
bin_PROGRAMS	+= bsqlodbc
endif


dist_bin_SCRIPTS = osql

freebcp_LDADD	= ../dblib/libsybdb.la ../replacements/libreplacements.la $(LTLIBICONV)
freebcp_SOURCES = freebcp.c freebcp.h

tsql_LDADD	= ../tds/libtds.la \
		  ../replacements/libreplacements.la \
		  $(LTLIBICONV) $(FREETDS_LIBGCC) $(READLINE_LIBS) $(NETWORK_LIBS)

bsqldb_LDADD	= ../dblib/libsybdb.la \
		  ../replacements/libreplacements.la \
		  $(LTLIBICONV)

if ODBC
bsqlodbc_CPPFLAGS	= $(ODBC_INC) $(AM_CPPFLAGS)
bsqlodbc_LDADD	= $(ODBCLIB) $(ODBCNODMLIBAPP) \
		  ../replacements/libreplacements.la $(LTLIBICONV) $(NETWORK_LIBS)
endif

defncopy_LDADD	= ../dblib/libsybdb.la \
		  ../replacements/libreplacements.la \
		  $(LTLIBICONV)

datacopy_SOURCES= datacopy.c
datacopy_LDADD	= ../dblib/libsybdb.la \
		  ../replacements/libreplacements.la \
		$(LTLIBICONV)

EXTRA_DIST = CMakeLists.txt
