include_HEADERS	=	cspublic.h ctpublic.h sybdb.h sybfront.h  \
			tdsutil.h bkpublic.h cstypes.h dblib.h syberror.h \
			ctlib.h sqldb.h sqlfront.h                        \
			tds.h tdsver.h tdsconvert.h tds_configs.h tdsodbc.h

EXTRA_DIST     =    tds_configs.h.in tds_configs.h \
		tdsiconv.h

all-local: tds_configs.h 

tds_configs.h: tds_configs.h.in Makefile
	@rm -f $@
	@echo "Making $@"
	@echo "/* Generated from tds_configs.h.in on `date` */" > $@;  \
	sed \
		-e 's;%SYSCONFDIR%;$(sysconfdir);' \
		$(srcdir)/tds_configs.h.in >> $@; \
	chmod 444 $@

maintainer-clean-local:
	rm -f tds_configs.h
