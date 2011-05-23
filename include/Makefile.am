AUTOMAKE_OPTIONS =	1.5.3

include_HEADERS	=	bkpublic.h \
			cspublic.h cstypes.h ctpublic.h \
			sqldb.h sqlfront.h \
			sybdb.h sybfront.h \
			syberror.h odbcss.h

nodist_include_HEADERS	= tds_sysdep_public.h

noinst_HEADERS	=	ctlib.h \
			dblib.h \
			fakepoll.h \
			md4.h md5.h des.h hmac_md5.h \
			replacements.h \
			replacements/readpassphrase.h \
			tds.h tdsver.h tdsconvert.h \
			tds_sysdep_private.h \
			tdsbytes.h \
			tdsiconv.h \
			tdsodbc.h \
			tdssrv.h \
			tdsstring.h \
			tdsthread.h \
			tds_configs.h \
			tdsproto.h


DISTCLEANFILES	=	tds_sysdep_public.h freetds_sysconfdir.h

freetds_sysconfdir.h: Makefile
	echo '#define FREETDS_SYSCONFDIR "$(sysconfdir)"' >$@

all-am: freetds_sysconfdir.h
