if ODBC
SUBDIRS	=	replacements tds ctlib dblib odbc apps server
DIST_SUBDIRS = pool $(SUBDIRS)
else
SUBDIRS	=	replacements tds ctlib dblib apps server
DIST_SUBDIRS = pool odbc $(SUBDIRS)
endif
