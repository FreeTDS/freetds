SUBDIRS		=	include src

EXTRA_DIST	=	interfaces freetds.conf samples/debug.c PWD BUGS \
				samples/README samples/dyntest.c samples/odbc.ini \
				samples/odbctest.php samples/odbctest.pl \
				samples/test.php samples/test.pl


DEFDIR		=	$(prefix)
ETC			=	$(sysconfdir)

install-data-local:
	$(mkinstalldirs) $(DEFDIR) $(ETC)
	if [ -f $(ETC)/freetds.conf ]; \
	then :; \
	else \
		$(INSTALL_DATA) freetds.conf $(ETC)/freetds.conf; \
	fi

test:
	@echo "The 'make test' option has been replaced with 'make check'";
