# $Id: Makefile.am,v 1.39 2006-03-27 07:22:54 jklowden Exp $
SUBDIRS		= unittests
AM_CPPFLAGS	=	-I$(top_srcdir)/include $(ODBC_INC)

lib_LTLIBRARIES	=	libtdsodbc.la
##EXTRA_LTLIBRARIES	=	libtdsodbc.la
libtdsodbc_la_SOURCES=	odbc.c connectparams.c convert_tds2sql.c \
	descriptor.c prepare_query.c odbc_util.c \
	native.c sql2tds.c error.c odbc_checks.c
libtdsodbc_la_LIBADD=	../tds/libtds_objects.la ../replacements/libreplacements.la $(ODBCINSTLIB) $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)
# -module is needed by Darwin (Mac OS X)
libtdsodbc_la_LDFLAGS = -export-symbols-regex '^(SQL|ODBCINST).*' $(FREETDS_SYMBOLIC) -module

## Need blank statement to avoid compiling odbc.c
odbc: $(EXTRA_LTLIBRARIES)
	@echo ''

