SUBDIRS		=	unittests

TDSDIR		=	../tds
TDSOBJECTS	=	$(TDSDIR)/mem.lo $(TDSDIR)/token.lo $(TDSDIR)/util.lo $(TDSDIR)/login.lo $(TDSDIR)/read.lo $(TDSDIR)/write.lo $(TDSDIR)/convert.lo $(TDSDIR)/numeric.lo $(TDSDIR)/config.lo $(TDSDIR)/query.lo $(TDSDIR)/iconv.lo $(TDSDIR)/locale.lo $(TDSDIR)/threadsafe.lo $(TDSDIR)/challenge.lo
lib_LTLIBRARIES	=	libct.la
libct_la_SOURCES=	ct.c cs.c blk.c ctutil.c
libct_la_LIBADD	=	$(TDSOBJECTS) $(NETWORK_LIBS)
INCLUDES	=	-I$(top_srcdir)/include
