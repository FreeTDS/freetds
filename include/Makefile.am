AUTOMAKE_OPTIONS	=	1.5.3

include_HEADERS	=	bkpublic.h cspublic.h cstypes.h ctpublic.h \
			dblib.h \
			sqldb.h sqlfront.h \
			sybdb.h sybfront.h \
			syberror.h \
			tds.h tdsconvert.h tdssrv.h \
			tdsver.h

nodist_include_HEADERS	=	tds_sysdep_public.h

noinst_HEADERS	=	tds_configs.h \
			tds_sysdep_private.h \
			md4.h des.h \
			replacements.h \
			tdsstring.h \
			tdsodbc.h \
			tdsiconv.h \
			ctlib.h

DISTCLEANFILES	=	tds_sysdep_public.h freetds_sysconfdir.h

freetds_sysconfdir.h: Makefile
	echo '#define FREETDS_SYSCONFDIR "$(sysconfdir)"' >$@

all-am: freetds_sysconfdir.h
