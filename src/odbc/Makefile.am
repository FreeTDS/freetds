# $Id: Makefile.am,v 1.41 2006-06-19 08:39:48 freddy77 Exp $
SUBDIRS		= unittests
AM_CPPFLAGS	=	-I$(top_srcdir)/include $(ODBC_INC)

lib_LTLIBRARIES	=	libtdsodbc.la
##EXTRA_LTLIBRARIES	=	libtdsodbc.la
if MINGW32
MINGW_SOURCES	= ../../win32/winlogin.c ../../win32/winsetup.c ../../win32/initnet.c
else
MINGW_SOURCES	=
endif
libtdsodbc_la_SOURCES=	odbc.c connectparams.c convert_tds2sql.c \
	descriptor.c prepare_query.c odbc_util.c \
	native.c sql2tds.c error.c odbc_checks.c $(MINGW_SOURCES)
libtdsodbc_la_LIBADD=	../tds/libtds.la ../replacements/libreplacements.la $(ODBCINSTLIB) $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)
# -module is needed by Darwin (Mac OS X)
libtdsodbc_la_LDFLAGS = -export-symbols-regex '^(SQL|ODBCINST).*' $(FREETDS_SYMBOLIC) -module

## Need blank statement to avoid compiling odbc.c
odbc: $(EXTRA_LTLIBRARIES)
	@echo ''

