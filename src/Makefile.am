if ODBC
SUBDIRS	=	tds ctlib dblib odbc apps server
DIST_SUBDIRS = pool $(SUBDIRS)
else
SUBDIRS	=	tds ctlib dblib apps server
DIST_SUBDIRS = pool odbc $(SUBDIRS)
endif
