# $Id: Makefile.am,v 1.39 2011-12-05 02:26:31 jklowden Exp $
SUBDIRS		=	. unittests
AM_CPPFLAGS	=	-I$(top_srcdir)/include

lib_LTLIBRARIES	=	libsybdb.la
libsybdb_la_SOURCES=	dblib.c dbutil.c rpc.c bcp.c xact.c dbpivot.c
if SYBASE_COMPAT
libsybdb_la_SOURCES +=	dbopen.c
endif
if MINGW32
libsybdb_la_SOURCES +=	winmain.c
endif
EXTRA_DIST	=	dbopen.c buffering.h CMakeLists.txt winmain.c dblib.def
# bumped for dbanydatecrack
libsybdb_la_LDFLAGS=	-version-info 6:0:1 $(FREETDS_SYMBOLIC)
if !MACOSX
libsybdb_la_LDFLAGS +=	-export-symbols-regex '^(db|bcp_|tdsdump_open|tdsdbopen|.*_xact|close_commit|open_commit|.?asprintf).*'
endif
libsybdb_la_LIBADD=	../tds/libtds.la ../replacements/libreplacements.la $(LTLIBICONV) $(FREETDS_LIBGCC)

