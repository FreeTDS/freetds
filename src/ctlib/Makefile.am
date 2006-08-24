# $Id: Makefile.am,v 1.27 2006-08-24 09:18:01 freddy77 Exp $
SUBDIRS		=	unittests
AM_CPPFLAGS	=	-I$(top_srcdir)/include

lib_LTLIBRARIES	=	libct.la
if MINGW32
MINGW_SOURCES   =	../../win32/initnet.c
else
MINGW_SOURCES   =
endif
libct_la_SOURCES=	ct.c cs.c blk.c ctutil.c $(MINGW_SOURCES)

# version bumped for 0.64, I hope we'll bump it less in the future -- freddy77
libct_la_LDFLAGS=	-version-info 4:0:0 -export-symbols-regex '^(cs_|ct_|blk_).*' $(FREETDS_SYMBOLIC)

libct_la_LIBADD	=	../tds/libtds.la ../replacements/libreplacements.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)

