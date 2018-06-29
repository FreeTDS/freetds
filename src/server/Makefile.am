AM_CPPFLAGS	=	-I$(top_srcdir)/include
noinst_LTLIBRARIES	=	libtdssrv.la
libtdssrv_la_SOURCES=	query.c server.c login.c
libtdssrv_la_LIBADD =	../tds/libtds.la ../replacements/libreplacements.la $(LTLIBICONV) $(FREETDS_LIBGCC)
noinst_PROGRAMS	= tdssrv
tdssrv_LDADD	= libtdssrv.la $(LTLIBICONV)
tdssrv_SOURCES	= unittest.c
EXTRA_DIST = CMakeLists.txt
