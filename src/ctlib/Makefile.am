SUBDIRS		=	unittests

TDSDIR		=	../tds
TDSSOURCES	=	mem.c token.c util.c login.c read.c write.c convert.c numeric.c config.c query.c iconv.c
lib_LTLIBRARIES	=	libct.la
libct_la_SOURCES=	ct.c cs.c blk.c ctutil.c
libct_la_LIBADD	=	$(patsubst %, $(TDSDIR)/%, \
				$(patsubst %.c, %.lo, $(TDSSOURCES)))
INCLUDES	=	-I$(top_srcdir)/include
