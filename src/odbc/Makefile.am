SUBDIRS		= unittests

lib_LTLIBRARIES	=	libtdsodbc.la
##EXTRA_LTLIBRARIES	=	libtdsodbc.la
libtdsodbc_la_SOURCES=	odbc.c connectparams.c convert_tds2sql.c \
	convert_sql2string.c descriptor.c prepare_query.c odbc_util.c \
	convert_sql2string.h convert_tds2sql.h odbc_util.h \
	prepare_query.h connectparams.h native.c \
	sql2tds.c sql2tds.h error.c
EXTRA_DIST = tables.sql
if UNIXODBC
libtdsodbc_la_LIBADD=	../tds/libtds_objects.la -lodbcinst $(NETWORK_LIBS) $(LIBICONV) ../replacements/libreplacements.la
else
libtdsodbc_la_LIBADD=	../tds/libtds_objects.la $(NETWORK_LIBS) $(LIBICONV) ../replacements/libreplacements.la
endif
INCLUDES	=	-I$(top_srcdir)/include -I$(ODBC_INC)
libtdsodbc_la_LDFLAGS = -export-symbols-regex '^(SQL|ODBCINST).*'

## Need blank statement to avoid compiling odbc.c
odbc: $(EXTRA_LTLIBRARIES)
	@echo ''

