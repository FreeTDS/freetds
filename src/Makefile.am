if ODBC
SUBDIRS	=	replacements tds ctlib dblib odbc apps server pool
DIST_SUBDIRS = $(SUBDIRS)
else
SUBDIRS	=	replacements tds ctlib dblib apps server pool
DIST_SUBDIRS = odbc $(SUBDIRS)
endif
