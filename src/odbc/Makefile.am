SUBDIRS		= unittests

lib_LTLIBRARIES	=	libtdsodbc.la
##EXTRA_LTLIBRARIES	=	libtdsodbc.la
libtdsodbc_la_SOURCES=	odbc.c connectparams.c convert_tds2sql.c \
	descriptor.c prepare_query.c odbc_util.c \
	convert_tds2sql.h odbc_util.h \
	prepare_query.h connectparams.h native.c \
	sql2tds.c sql2tds.h error.c \
	odbc_checks.c odbc_checks.h
libtdsodbc_la_LIBADD=	../tds/libtds_objects.la $(ODBCINSTLIB) $(NETWORK_LIBS) $(LIBICONV) ../replacements/libreplacements.la
INCLUDES	=	-I$(top_srcdir)/include $(ODBC_INC)
libtdsodbc_la_LDFLAGS = -export-symbols-regex '^(SQL|ODBCINST).*' -Wl,-Bsymbolic

## Need blank statement to avoid compiling odbc.c
odbc: $(EXTRA_LTLIBRARIES)
	@echo ''

