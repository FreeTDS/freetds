# $Id: Makefile.am,v 1.35 2006-08-24 09:18:01 freddy77 Exp $
SUBDIRS		=	unittests
AM_CPPFLAGS	=	-I$(top_srcdir)/include

lib_LTLIBRARIES	=	libsybdb.la
if SYBASE_COMPAT
DBOPEN=dbopen.c
endif
if MINGW32
MINGW_SOURCES   =	../../win32/initnet.c
else
MINGW_SOURCES   =
endif
EXTRA_DIST	=	dbopen.c buffering.h
libsybdb_la_SOURCES=	dblib.c dbutil.c rpc.c bcp.c xact.c $(DBOPEN) $(MINGW_SOURCES)
# version bumped for 0.63, I hope we'll bump it less in the future -- freddy77
libsybdb_la_LDFLAGS=	-version-info 5:0:0 -export-symbols-regex '^(db|bcp_|tdsdump_open|tdsdbopen|.*_xact|close_commit|open_commit|.?asprintf).*' $(FREETDS_SYMBOLIC)
libsybdb_la_LIBADD=	../tds/libtds.la ../replacements/libreplacements.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)

