# $Id: Makefile.am,v 1.13 2004-12-13 10:59:59 freddy77 Exp $
lib_LTLIBRARIES	=	libtdssrv.la
libtdssrv_la_SOURCES=	query.c server.c login.c
# bump server version whenever bumping libtds version
libtdssrv_la_LDFLAGS=	-version-info 2:0:0
libtdssrv_la_LIBADD =	../tds/libtds_objects.la ../replacements/libreplacements.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)
INCLUDES	=	-I$(top_srcdir)/include
noinst_PROGRAMS	= tdssrv
tdssrv_LDADD	= libtdssrv.la $(NETWORK_LIBS)
tdssrv_SOURCES	= unittest.c
