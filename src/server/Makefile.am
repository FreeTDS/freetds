# $Id: Makefile.am,v 1.11.2.2 2004-12-09 08:43:22 freddy77 Exp $
lib_LTLIBRARIES	=	libtdssrv.la
libtdssrv_la_SOURCES=	query.c server.c login.c
libtdssrv_la_LDFLAGS=	-version-info 1:0:0
libtdssrv_la_LIBADD =	../tds/libtds_objects.la ../replacements/libreplacements.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)
INCLUDES	=	-I$(top_srcdir)/include
noinst_PROGRAMS	= tdssrv
tdssrv_LDADD	= libtdssrv.la $(NETWORK_LIBS)
tdssrv_SOURCES	= unittest.c
