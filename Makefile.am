SUBDIRS		=	include src doc

EXTRA_DIST	=	interfaces samples/debug.c PWD BUGS \
		samples/README samples/dyntest.c samples/odbc.ini \
		samples/odbctest.php samples/odbctest.pl samples/test.php \
		samples/test.pl \
		freetds.conf locales.conf acconfig.h \
		autogen.sh freetds.spec.in freetds.spec

DEFDIR		=	$(prefix)
ETC		=	$(sysconfdir)

install-data-local:
	$(mkinstalldirs) $(DEFDIR) $(ETC)
	if [ -f $(ETC)/freetds.conf ]; \
	then :; \
	else \
		$(INSTALL_DATA) $(srcdir)/freetds.conf $(ETC)/freetds.conf; \
	fi
	if [ -f $(ETC)/locales.conf ]; \
	then :; \
	else \
		$(INSTALL_DATA) $(srcdir)/locales.conf $(ETC)/locales.conf; \
	fi

test:
	@echo "The 'make test' option has been replaced with 'make check'";

all-redirect:
	if test ! -f PWD; then cp $(srcdir)/PWD PWD; fi

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-`date +"%Y%m%d"`
