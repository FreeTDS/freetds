SUBDIRS		=	unittests

TDSDIR		=	../tds
TDSOBJECTS	=	$(TDSDIR)/mem.lo $(TDSDIR)/token.lo $(TDSDIR)/util.lo $(TDSDIR)/login.lo $(TDSDIR)/read.lo $(TDSDIR)/write.lo $(TDSDIR)/convert.lo $(TDSDIR)/numeric.lo $(TDSDIR)/config.lo $(TDSDIR)/query.lo $(TDSDIR)/iconv.lo $(TDSDIR)/locale.lo
lib_LTLIBRARIES	=	libct.la
libct_la_SOURCES=	ct.c cs.c blk.c ctutil.c
libct_la_LIBADD	=	$(TDSOBJECTS)
INCLUDES	=	-I$(top_srcdir)/include
