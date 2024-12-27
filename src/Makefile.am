SUBDIRS	     = utils replacements tds
DIST_SUBDIRS = utils replacements tds server pool ctlib dblib odbc apps

if INCPOOL
SUBDIRS += server pool
else !INCPOOL
if INCSERVER
SUBDIRS += server
endif
endif

SUBDIRS	+= ctlib dblib

if ODBC
SUBDIRS	+= odbc
endif

if INCAPPS
SUBDIRS += apps
endif
