# $Id: Makefile.am,v 1.23 2004-12-13 10:59:58 freddy77 Exp $
SUBDIRS		=	unittests

lib_LTLIBRARIES	=	libct.la
libct_la_SOURCES=	ct.c cs.c blk.c ctutil.c

# version bumped for 0.63, I hope we'll bump it less in the future -- freddy77
libct_la_LDFLAGS=	-version-info 3:0:0 -export-symbols-regex '^(cs_|ct_|blk_).*' $(FREETDS_SYMBOLIC)

libct_la_LIBADD	=	../tds/libtds_objects.la ../replacements/libreplacements.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)

INCLUDES	=	-I$(top_srcdir)/include
