SUBDIRS		=	unittests

TDSDIR		=	../tds
TDSOBJECTS	=	$(TDSDIR)/mem.lo $(TDSDIR)/token.lo $(TDSDIR)/util.lo $(TDSDIR)/login.lo $(TDSDIR)/read.lo $(TDSDIR)/write.lo $(TDSDIR)/convert.lo $(TDSDIR)/numeric.lo $(TDSDIR)/config.lo $(TDSDIR)/query.lo $(TDSDIR)/iconv.lo $(TDSDIR)/locale.lo $(TDSDIR)/threadsafe.lo $(TDSDIR)/challenge.lo $(TDSDIR)/asprintf.lo
lib_LTLIBRARIES	=	libsybdb.la
if SYBASE_COMPAT
DBOPEN=dbopen.c
endif
EXTRA_DIST	=	dbopen.c
libsybdb_la_SOURCES=	dblib.c dbutil.c rpc.c bcp.c xact.c $(DBOPEN)
libsybdb_la_LDFLAGS=	-version-info 2 0 0
libsybdb_la_LIBADD=	$(TDSOBJECTS) $(NETWORK_LIBS) $(LIBICONV)
INCLUDES	=	-I$(top_srcdir)/include
