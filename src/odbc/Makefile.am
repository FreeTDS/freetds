include_HEADERS = connectparams.h odbc_util.h convert_tds2sql.h prepare_query.h convert_sql2string.h
TDSDIR		=	../tds
TDSOBJECTS	=	$(TDSDIR)/mem.lo $(TDSDIR)/token.lo \
		$(TDSDIR)/util.lo $(TDSDIR)/login.lo $(TDSDIR)/read.lo \
		$(TDSDIR)/write.lo $(TDSDIR)/convert.lo \
		$(TDSDIR)/numeric.lo $(TDSDIR)/config.lo \
		$(TDSDIR)/query.lo $(TDSDIR)/iconv.lo \
		$(TDSDIR)/locale.lo $(TDSDIR)/threadsafe.lo \
		$(TDSDIR)/challenge.lo
lib_LTLIBRARIES	=	libtdsodbc.la
##EXTRA_LTLIBRARIES	=	libtdsodbc.la
libtdsodbc_la_SOURCES=	odbc.c connectparams.c convert_tds2sql.c \
	convert_sql2string.c prepare_query.c odbc_util.c \
	convert_sql2string.h convert_tds2sql.h odbc_util.h \
	prepare_query.h
EXTRA_DIST = tables.sql
if UNIXODBC
libtdsodbc_la_LIBADD=	$(TDSOBJECTS) -lodbcinst $(NETWORK_LIBS) $(LIBICONV)
else
libtdsodbc_la_LIBADD=	$(TDSOBJECTS) $(NETWORK_LIBS)
endif
INCLUDES	=	-I$(top_srcdir)/include -I$(ODBC_INC)

## Need blank statement to avoid compiling odbc.c
odbc: $(EXTRA_LTLIBRARIES)
	@echo ''

