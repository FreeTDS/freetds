SUBDIRS	     = replacements tds ctlib dblib
DIST_SUBDIRS = $(SUBDIRS)

if ODBC
SUBDIRS	+= odbc
else
DIST_SUBDIRS += odbc
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
