# $Id: Makefile.am,v 1.29 2011-06-08 09:25:53 freddy77 Exp $
SUBDIRS		=	. unittests
AM_CPPFLAGS	=	-I$(top_srcdir)/include

lib_LTLIBRARIES	=	libct.la
libct_la_SOURCES=	ct.c cs.c blk.c ctutil.c
if MINGW32
libct_la_SOURCES +=	winmain.c
endif

# version bumped for 0.64, I hope we'll bump it less in the future -- freddy77
libct_la_LDFLAGS =	-version-info 4:0:0 $(FREETDS_SYMBOLIC)
if !MACOSX
libct_la_LDFLAGS +=	-export-symbols-regex '^(cs_|ct_|blk_).*'
endif

libct_la_LIBADD	=	../tds/libtds.la ../replacements/libreplacements.la $(LTLIBICONV) $(FREETDS_LIBGCC)

EXTRA_DIST		= ctlib.def CMakeLists.txt winmain.c

