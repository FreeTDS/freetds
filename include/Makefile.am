include_HEADERS	=	bkpublic.h cspublic.h cstypes.h ctlib.h ctpublic.h \
			dblib.h \
			sqldb.h sqlfront.h \
			sybdb.h sybfront.h \
			syberror.h \
			tds.h tdsconvert.h tdsodbc.h tdssrv.h tdsutil.h
			tdsver.h \
			tds_sysdep_public.h

EXTRA_DIST	=	tds_configs.h \
			tdsiconv.h md4.h des.h

all-local: tds_sysdep_private.h tds_sysdep_public.h

maintainer-clean-local:
	rm -f tds_sysdep_private.h tds_sysdep_public.h
