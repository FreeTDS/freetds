# $Id: Makefile.am,v 1.9 2004-04-14 07:52:55 freddy77 Exp $
lib_LTLIBRARIES	=	libtdssrv.la
libtdssrv_la_SOURCES=	query.c server.c login.c
libtdssrv_la_LDFLAGS=	-version-info 1:0:0
libtdssrv_la_LIBADD =	../tds/libtds_objects.la $(NETWORK_LIBS) $(LIBICONV) ../replacements/libreplacements.la
INCLUDES	=	-I$(top_srcdir)/include
bin_noinst_PROGRAMS	= tdssrv
tdssrv_LDADD	= libtdssrv.la $(NETWORK_LIBS)
tdssrv_SOURCES	= unittest.c
