SUBDIRS			=	unittests

INCLUDES		=	-I$(top_srcdir)/include
noinst_LTLIBRARIES	=	libtds_objects.la
libtds_objects_la_SOURCES=	mem.c token.c util.c login.c read.c write.c convert.c numeric.c config.c query.c iconv.c locale.c challenge.c threadsafe.c vstrbuild.c md4.c des.c
libtds_objects_la_LDFLAGS=
libtds_objects_la_LIBADD=

lib_LTLIBRARIES		=	libtds.la
libtds_la_SOURCES	=
libtds_la_LDFLAGS	= -version-info 1:1:0
libtds_la_LIBADD 	= libtds_objects.la $(NETWORK_LIBS) $(LIBICONV) ../replacements/libreplacements.la
EXTRA_DIST	= tds_willconvert.h

if HAVE_DOXYGEN
doxy: tds.dox
	$(DOXYGEN) tds.dox
endif
