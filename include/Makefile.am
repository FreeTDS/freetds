AUTOMAKE_OPTIONS = 1.5.3

NULL =

SUBDIRS	= freetds

include_HEADERS	= \
	bkpublic.h \
	cspublic.h \
	cstypes.h \
	ctpublic.h \
	sqldb.h \
	sqlfront.h \
	sybdb.h \
	sybfront.h \
	syberror.h \
	odbcss.h \
	$(NULL)

nodist_include_HEADERS = tds_sysdep_public.h

noinst_HEADERS = \
	ctlib.h \
	dblib.h \
	replacements.h \
	replacements/poll.h \
	replacements/readpassphrase.h \
	$(NULL)

DISTCLEANFILES = tds_sysdep_public.h
