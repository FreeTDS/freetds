SUBDIRS	     = utils replacements tds ctlib dblib
DIST_SUBDIRS = $(SUBDIRS)

if ODBC
SUBDIRS	+= odbc
else
DIST_SUBDIRS += odbc
endif

if INCPOOL
SUBDIRS += server pool
else
DIST_SUBDIRS += server pool
if INCSERVER
SUBDIRS += server
else
DIST_SUBDIRS += server
endif
endif

if INCAPPS
SUBDIRS += apps
else
DIST_SUBDIRS += apps
endif
