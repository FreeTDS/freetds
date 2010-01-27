# $Id: Makefile.am,v 1.17 2010-01-27 03:21:08 jklowden Exp $
AM_CPPFLAGS	=	-I$(top_srcdir)/include
noinst_LTLIBRARIES	=	libtdssrv.la
libtdssrv_la_SOURCES=	query.c server.c login.c
libtdssrv_la_LIBADD =	../tds/libtds.la ../replacements/libreplacements.la $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)
noinst_PROGRAMS	= tdssrv
tdssrv_LDADD	= libtdssrv.la $(LIBICONV) $(NETWORK_LIBS)
tdssrv_SOURCES	= unittest.c
