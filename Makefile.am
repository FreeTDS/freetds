# version $Id: Makefile.am,v 1.23 2002-10-31 14:48:22 freddy77 Exp $
SUBDIRS		=	include src doc samples

EXTRA_DIST	=	interfaces PWD.in BUGS \
		freetds.conf locales.conf \
		autogen.sh freetds.spec.in freetds.spec

ETC            =       $(DESTDIR)$(sysconfdir)

install-data-local:
	$(mkinstalldirs) $(ETC)
	if test ! -f $(ETC)/freetds.conf; then \
		$(INSTALL_DATA) $(srcdir)/freetds.conf $(ETC)/freetds.conf; \
	fi
	if test ! -f $(ETC)/locales.conf; then \
		$(INSTALL_DATA) $(srcdir)/locales.conf $(ETC)/locales.conf; \
	fi

if DISTCHECK_BUILD
uninstall-local:
	$(RM) $(ETC)/freetds.conf $(ETC)/locales.conf $(ETC)/pool.conf PWD
endif

test:
	@echo "The 'make test' option has been replaced with 'make check'";

all:
	if test ! -f PWD; then cp $(srcdir)/PWD.in PWD; fi

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-`date +"%Y%m%d"`

doxy:
	cd src/tds; $(MAKE) doxy

