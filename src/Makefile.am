SUBDIRS	     = utils replacements tds ctlib dblib
DIST_SUBDIRS = utils replacements tds ctlib dblib \
	odbc server pool apps

if ODBC
SUBDIRS	+= odbc
endif

if INCPOOL
SUBDIRS += server pool
else !INCPOOL
if INCSERVER
SUBDIRS += server
endif
endif

if INCAPPS
SUBDIRS += apps
endif
