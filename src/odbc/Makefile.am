include_HEADERS = connectparams.h odbc_util.h convert_tds2sql.h prepare_query.h convert_sql2string.h
TDSDIR		=	../tds
TDSSOURCES	=	mem.c token.c util.c login.c read.c write.c convert.c numeric.c config.c query.c iconv.c locale.c
lib_LTLIBRARIES	=	libtdsodbc.la
##EXTRA_LTLIBRARIES	=	libtdsodbc.la
libtdsodbc_la_SOURCES=	odbc.c connectparams.c convert_tds2sql.c \
	convert_sql2string.c prepare_query.c odbc_util.c \
	convert_sql2string.h convert_tds2sql.h odbc_util.h \
	prepare_query.h
EXTRA_DIST = tables.sql
if UNIXODBC
libtdsodbc_la_LIBADD=	$(patsubst %, $(TDSDIR)/%, $(patsubst %.c, %.lo, $(TDSSOURCES))) -lodbcinst $(NETWORK_LIBS) $(LIBICONV)
else
libtdsodbc_la_LIBADD=	$(patsubst %, $(TDSDIR)/%, $(patsubst %.c, %.lo, $(TDSSOURCES))) $(NETWORK_LIBS)
endif
INCLUDES	=	-I$(top_srcdir)/include -I$(ODBC_INC)

## Need blank statement to avoid compiling odbc.c
odbc: $(EXTRA_LTLIBRARIES)
	@echo ''

