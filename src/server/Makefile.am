TDSDIR		=	../tds
TDSSOURCES	=	mem.c token.c util.c login.c read.c write.c convert.c numeric.c config.c query.c iconv.c
lib_LTLIBRARIES	=	libtdssrv.la
##EXTRA_LTLIBRARIES	=	libtdssrv.la
libtdssrv_la_SOURCES=	query.c server.c login.c
libtdssrv_la_LDFLAGS=	-rpath $(libdir)
libtdssrv_la_LIBADD=	$(patsubst %, $(TDSDIR)/%, \
				$(patsubst %.c, %.lo, $(TDSSOURCES)))
INCLUDES	=	-I$(top_srcdir)/include

## Need a blank statement to avoid compiling server.c
server: $(EXTRA_LTLIBRARIES)
	@echo ''

