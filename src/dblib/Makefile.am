SUBDIRS		=	unittests

lib_LTLIBRARIES	=	libsybdb.la
if SYBASE_COMPAT
DBOPEN=dbopen.c
endif
EXTRA_DIST	=	dbopen.c
libsybdb_la_SOURCES=	dblib.c dbutil.c rpc.c bcp.c xact.c $(DBOPEN)
libsybdb_la_LDFLAGS=	-version-info 3:0:1
libsybdb_la_LIBADD=	../tds/libtds_objects.la $(NETWORK_LIBS) $(LIBICONV) ../replacements/libreplacements.la

INCLUDES	=	-I$(top_srcdir)/include
