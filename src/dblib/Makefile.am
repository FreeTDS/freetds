# $Id: Makefile.am,v 1.30 2004-12-07 17:51:30 freddy77 Exp $
SUBDIRS		=	unittests

lib_LTLIBRARIES	=	libsybdb.la
if SYBASE_COMPAT
DBOPEN=dbopen.c
endif
EXTRA_DIST	=	dbopen.c
libsybdb_la_SOURCES=	dblib.c dbutil.c rpc.c bcp.c xact.c $(DBOPEN)
# version bumped for 0.63, I hope we'll bump it less in the future -- freddy77
libsybdb_la_LDFLAGS=	-version-info 5:0:0 -export-symbols-regex '^(db|bcp_|tdsdump_open|tdsdbopen|.*_xact|close_commit|open_commit|.?asprintf).*' $(FREETDS_SYMBOLIC)
libsybdb_la_LIBADD=	../replacements/libreplacements.la ../tds/libtds_objects.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)

INCLUDES	=	-I$(top_srcdir)/include
