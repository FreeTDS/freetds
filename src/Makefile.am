if INCAPPS
APPS = apps
endif
if INCPOOL
POOL = pool
SERVER = server
else !INCPOOL
if INCSERVER
SERVER = server 
endif
endif
if ODBC
SUBDIRS	=	replacements tds ctlib dblib odbc $(APPS) $(SERVER) $(POOL)
DIST_SUBDIRS = $(SUBDIRS)
else
SUBDIRS	=	replacements tds ctlib dblib $(APPS) $(SERVER) $(POOL)
DIST_SUBDIRS = odbc $(SUBDIRS)
endif
