# $Id: Makefile.am,v 1.14 2006-03-27 07:22:54 jklowden Exp $
AM_CPPFLAGS	=	-I$(top_srcdir)/include
lib_LTLIBRARIES	=	libtdssrv.la
libtdssrv_la_SOURCES=	query.c server.c login.c
# bump server version whenever bumping libtds version
libtdssrv_la_LDFLAGS=	-version-info 2:0:0
libtdssrv_la_LIBADD =	../tds/libtds_objects.la ../replacements/libreplacements.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)
noinst_PROGRAMS	= tdssrv
tdssrv_LDADD	= libtdssrv.la $(NETWORK_LIBS)
tdssrv_SOURCES	= unittest.c
