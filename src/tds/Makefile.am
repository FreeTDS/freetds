SUBDIRS			=	unittests

lib_LTLIBRARIES		=	libtds.la @LIBICONV@
libtds_la_SOURCES	=	mem.c token.c util.c login.c read.c write.c convert.c numeric.c config.c query.c iconv.c locale.c challenge.c threadsafe.c
INCLUDES		=	-I$(top_srcdir)/include
libtds_la_LIBADD 	= $(NETWORK_LIBS)

EXTRA_DIST	= tds_willconvert.h

