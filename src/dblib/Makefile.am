SUBDIRS		=	unittests

TDSDIR		=	../tds
TDSSOURCES	=	mem.c token.c util.c login.c read.c write.c convert.c numeric.c config.c query.c iconv.c locale.c
lib_LTLIBRARIES	=	libsybdb.la
libsybdb_la_SOURCES=	dblib.c dbutil.c rpc.c bcp.c xact.c
libsybdb_la_LDFLAGS=	-version-info 1 0 0
libsybdb_la_LIBADD=	$(patsubst %, $(TDSDIR)/%, \
				$(patsubst %.c, %.lo, $(TDSSOURCES)))
INCLUDES	=	-I$(top_srcdir)/include
