SUBDIRS		=	include src

EXTRA_DIST	=	interfaces freetds.conf samples/debug.c PWD BUGS

DEFDIR		=	$(prefix)
ETC			=	$(sysconfdir)

install-data-local:
	$(mkinstalldirs) $(DEFDIR) $(ETC)
	if [ -f $(DEFDIR)/interfaces ]; \
	then :; \
	else \
		$(INSTALL_DATA) interfaces $(DEFDIR); \
	fi
	if [ -f $(ETC)/freetds.conf ]; \
	then :; \
	else \
		$(INSTALL_DATA) freetds.conf $(ETC)/freetds.conf; \
	fi

test:
	@echo "The 'make test' option has been replaced with 'make check'";
