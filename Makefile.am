# version $Id: Makefile.am,v 1.20 2002-10-15 18:54:25 castellano Exp $
SUBDIRS		=	include src doc samples

EXTRA_DIST	=	interfaces PWD.in BUGS \
		autogen.sh freetds.spec.in freetds.spec

dist_sysconf_DATA	=	freetds.conf locales.conf

test:
	@echo "The 'make test' option has been replaced with 'make check'";

all-redirect:
	if test ! -f PWD; then cp $(srcdir)/PWD.in PWD; fi

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-`date +"%Y%m%d"`

doxy:
	cd src/tds; $(MAKE) doxy

