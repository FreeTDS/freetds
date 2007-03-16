# $Id: Makefile.am,v 1.62 2007-03-16 15:34:09 freddy77 Exp $

SUBDIRS			=	unittests
AM_CPPFLAGS		=	-I$(top_srcdir)/include

noinst_LTLIBRARIES	=	libtds.la
libtds_la_SOURCES=	mem.c token.c util.c login.c read.c \
	write.c convert.c numeric.c config.c query.c iconv.c \
	locale.c challenge.c threadsafe.c vstrbuild.c md4.c md5.c \
	des.c tdsstring.c getmac.c data.c net.c \
	tds_checks.c tds_checks.h enum_cap.h log.c
libtds_la_LDFLAGS=
libtds_la_LIBADD=

noinst_HEADERS		= tds_willconvert.h encodings.h num_limits.h
EXTRA_DIST		= tds_willconvert.h encodings.h num_limits.h

if HAVE_DOXYGEN
doxyfile: $(srcdir)/tds.dox
	cp $(srcdir)/tds.dox doxyfile
	
## The doxygen configuration file understands environment variables.  

doxy: doxyfile  $(libtds_la_SOURCES)
	SRCDIR=$(srcdir); \
	TOP_SRCDIR=$(top_srcdir); \
	export SRCDIR TOP_SRCDIR; \
	$(DOXYGEN) doxyfile
endif

# Perl is needed to build from a CVS checkout.  
# Perl is *not* needed to build from a tarball.  

## We have to build num_limits.h because automake can't infer the
## dependency (on num_limits.pl).  

if HAVE_PERL_SOURCES
BUILT_SOURCES = tds_willconvert.h encodings.h num_limits.h

clean-local: 
	cd $(srcdir) && rm -f $(EXTRA_DIST)
	
tds_willconvert.h: tds_willconvert.pl Makefile
	perl $(srcdir)/tds_willconvert.pl > $@.tmp
	mv $@.tmp $@

encodings.h: encodings.pl alternative_character_sets.h Makefile
	perl $(srcdir)/encodings.pl $(srcdir) > $@.tmp 2> /dev/null
	mv $@.tmp $@

num_limits.h: num_limits.pl Makefile
	perl $(srcdir)/num_limits.pl > $@.tmp
	mv $@.tmp $@
endif
