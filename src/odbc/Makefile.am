include_HEADERS = connectparams.h
TDSDIR		=	../tds
TDSSOURCES	=	mem.c token.c util.c login.c read.c write.c convert.c numeric.c config.c query.c
lib_LTLIBRARIES	=	libtdsodbc.la
##EXTRA_LTLIBRARIES	=	libtdsodbc.la
libtdsodbc_la_SOURCES=	odbc.c connectparams.c
libtdsodbc_la_LIBADD=	$(patsubst %, $(TDSDIR)/%, \
				$(patsubst %.c, %.lo, $(TDSSOURCES))) -lglib
INCLUDES	=	-I$(top_srcdir)/include `glib-config --cflags glib` -I$(ODBC_INC)

## Need blank statement to avoid compiling odbc.c
odbc: $(EXTRA_LTLIBRARIES)
	@echo ''

