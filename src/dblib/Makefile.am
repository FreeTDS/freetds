SUBDIRS		=	unittests

lib_LTLIBRARIES	=	libsybdb.la
if SYBASE_COMPAT
DBOPEN=dbopen.c
endif
EXTRA_DIST	=	dbopen.c
libsybdb_la_SOURCES=	dblib.c dbutil.c rpc.c bcp.c xact.c $(DBOPEN)
libsybdb_la_LDFLAGS=	-version-info 2 0 0
libsybdb_la_LIBADD=	$(top_srcdir)/src/tds/libtds_objects.la $(NETWORK_LIBS) $(LIBICONV) $(top_srcdir)/src/replacements/libreplacements.la

INCLUDES	=	-I$(top_srcdir)/include
