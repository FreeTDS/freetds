include_HEADERS	=	cspublic.h ctpublic.h sybdb.h sybfront.h  \
			tdsutil.h bkpublic.h cstypes.h dblib.h syberror.h \
			ctlib.h sqldb.h sqlfront.h                        \
			tds.h tdsver.h tdsconvert.h tds_configs.h tdsodbc.h \
			tdssrv.h

EXTRA_DIST     =    tds_configs.h.in tds_configs.h \
		tdsiconv.h md4.h des.h

all-local: tds_configs.h 

maintainer-clean-local:
	rm -f tds_configs.h
