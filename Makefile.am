# version $Id: Makefile.am,v 1.29 2003-04-13 14:07:11 jklowden Exp $

## SUBDIRS determines in which directories automake will generate a Makefile
## See also AC_OUTPUT in configure.in

SUBDIRS		=	include src doc samples win32

EXTRA_DIST	=	interfaces PWD.in BUGS \
		freetds.conf locales.conf \
		autogen.sh freetds.spec.in freetds.spec vms

ETC            =       $(DESTDIR)$(sysconfdir)

install-data-local:
	$(mkinstalldirs) $(ETC)
	if test ! -f $(ETC)/freetds.conf; then \
		$(INSTALL_DATA) $(srcdir)/freetds.conf $(ETC)/freetds.conf; \
	fi
	if test ! -f $(ETC)/locales.conf; then \
		$(INSTALL_DATA) $(srcdir)/locales.conf $(ETC)/locales.conf; \
	fi

# enable some cheat so make distcheck work (see below)
DISTCHECK_CONFIGURE_FLAGS	=	\
	--enable-distcheck-build --enable-extra-checks

# do not uninstall configuration files (they may have been changed)
if DISTCHECK_BUILD
uninstall-local:
	$(RM) $(ETC)/freetds.conf $(ETC)/locales.conf $(ETC)/pool.conf PWD
endif

test:
	@echo "The 'make test' option has been replaced with 'make check'";

all:
if DISTCHECK_BUILD
# if we are inside a make distcheck copy our real password file
	if test ! -f PWD -a -f $(srcdir)/../PWD; then cp $(srcdir)/../PWD PWD; fi
endif
# this prevent the store of passwords in CVS
	if test ! -f PWD; then cp $(srcdir)/PWD.in PWD; fi

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-`date +"%Y%m%d"`

if HAVE_DOXYGEN
doxyfile: $(top_srcdir)/tds.dox
	cp $(top_srcdir)/tds.dox doxyfile
	
## The doxygen configuration file understands environment variables.  

doxy: doxyfile  $(SUBDIRS)
	SRCDIR=$(top_srcdir); \
	export SRCDIR; \
	$(DOXYGEN) doxyfile
endif
