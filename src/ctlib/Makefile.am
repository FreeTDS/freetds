SUBDIRS		=	unittests

lib_LTLIBRARIES	=	libct.la
libct_la_SOURCES=	ct.c cs.c blk.c ctutil.c
libct_la_LDFLAGS=	-version-info 2:0:0
libct_la_LIBADD	=	../tds/libtds_objects.la $(NETWORK_LIBS) $(LIBICONV) ../replacements/libreplacements.la

INCLUDES	=	-I$(top_srcdir)/include
