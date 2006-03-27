# $Id: Makefile.am,v 1.25 2006-03-27 07:22:54 jklowden Exp $
SUBDIRS		=	unittests
AM_CPPFLAGS	=	-I$(top_srcdir)/include

lib_LTLIBRARIES	=	libct.la
libct_la_SOURCES=	ct.c cs.c blk.c ctutil.c

# version bumped for 0.64, I hope we'll bump it less in the future -- freddy77
libct_la_LDFLAGS=	-version-info 4:0:0 -export-symbols-regex '^(cs_|ct_|blk_).*' $(FREETDS_SYMBOLIC)

libct_la_LIBADD	=	../tds/libtds_objects.la ../replacements/libreplacements.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)

