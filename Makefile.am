# version $Id: Makefile.am,v 1.17 2002-09-28 12:47:01 freddy77 Exp $
SUBDIRS		=	include src doc

EXTRA_DIST	=	interfaces samples/debug.c PWD.in BUGS \
		samples/README samples/dyntest.c samples/odbc.ini \
		samples/odbctest.php samples/odbctest.pl samples/test.php \
		samples/test.pl \
		freetds.conf locales.conf acconfig.h \
		autogen.sh freetds.spec.in freetds.spec

ETC            =       $(DESTDIR)/$(sysconfdir)

install-data-local:
	$(mkinstalldirs) $(ETC)
	if test ! -f $(ETC)/freetds.conf; then \
		$(INSTALL_DATA) $(srcdir)/freetds.conf $(ETC)/freetds.conf; \
	fi
	if test ! -f $(ETC)/locales.conf; then \
		$(INSTALL_DATA) $(srcdir)/locales.conf $(ETC)/locales.conf; \
	fi

test:
	@echo "The 'make test' option has been replaced with 'make check'";

all-redirect:
	if test ! -f PWD; then cp $(srcdir)/PWD.in PWD; fi

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-`date +"%Y%m%d"`

doxy:
	cd src/tds; $(MAKE) doxy

