lib_LTLIBRARIES	=	libtdssrv.la
libtdssrv_la_SOURCES=	query.c server.c login.c
libtdssrv_la_LDFLAGS=	-version-info 1:0:0
libtdssrv_la_LIBADD =	../tds/libtds_objects.la $(NETWORK_LIBS) $(LIBICONV) ../replacements/libreplacements.la
INCLUDES	=	-I$(top_srcdir)/include
EXTRA_DIST = unittest.c
