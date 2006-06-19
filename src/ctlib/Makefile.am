# $Id: Makefile.am,v 1.26 2006-06-19 08:39:48 freddy77 Exp $
SUBDIRS		=	unittests
AM_CPPFLAGS	=	-I$(top_srcdir)/include

lib_LTLIBRARIES	=	libct.la
libct_la_SOURCES=	ct.c cs.c blk.c ctutil.c

# version bumped for 0.64, I hope we'll bump it less in the future -- freddy77
libct_la_LDFLAGS=	-version-info 4:0:0 -export-symbols-regex '^(cs_|ct_|blk_).*' $(FREETDS_SYMBOLIC)

libct_la_LIBADD	=	../tds/libtds.la ../replacements/libreplacements.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)

