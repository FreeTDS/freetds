SUBDIRS			=	unittests

lib_LTLIBRARIES		=	libtds.la
libtds_la_SOURCES	=	mem.c token.c util.c login.c read.c write.c convert.c numeric.c config.c query.c iconv.c locale.c challenge.c threadsafe.c asprintf.c
libtds_la_LDFLAGS	=    -version-info 1 0 0
INCLUDES			=	-I$(top_srcdir)/include
libtds_la_LIBADD 	= $(NETWORK_LIBS) $(LIBICONV)

EXTRA_DIST	= tds_willconvert.h

