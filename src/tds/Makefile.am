# $Id: Makefile.am,v 1.46 2005-04-01 20:36:50 jklowden Exp $

SUBDIRS			=	unittests

INCLUDES		=	-I$(top_srcdir)/include
noinst_LTLIBRARIES	=	libtds_objects.la
libtds_objects_la_SOURCES=	mem.c token.c util.c login.c read.c \
	write.c convert.c numeric.c config.c query.c iconv.c \
	locale.c challenge.c threadsafe.c vstrbuild.c md4.c md5.c \
	des.c tdsstring.c getmac.c data.c net.c \
	tds_checks.c tds_checks.h 
libtds_objects_la_LDFLAGS=
libtds_objects_la_LIBADD=

lib_LTLIBRARIES		=	libtds.la
libtds_la_SOURCES	=

# changed for 0.64
#                      bump server version whenever bumping libtds version
libtds_la_LDFLAGS	= -version-info 5:0:0 \
			  -export-symbols-regex '^(tds_|tdsdump_|tds5_|tds7_).*'
libtds_la_LIBADD 	= libtds_objects.la ../replacements/libreplacements.la \
			  $(NETWORK_LIBS) $(LIBICONV) $(FREETDS_LIBGCC)

noinst_HEADERS		= tds_willconvert.h character_sets.h \
			  alternative_character_sets.h sybase_character_sets.h \
			  encodings.h num_limits.h

if HAVE_DOXYGEN
doxyfile: $(srcdir)/tds.dox
	cp $(srcdir)/tds.dox doxyfile
	
## The doxygen configuration file understands environment variables.  

doxy: doxyfile  $(libtds_objects_la_SOURCES)
	SRCDIR=$(srcdir); \
	TOP_SRCDIR=$(top_srcdir); \
	export SRCDIR TOP_SRCDIR; \
	$(DOXYGEN) doxyfile
endif

# Perl is needed to build from a CVS checkout.  
# Perl is *not* needed to build from a tarball.  
# If num_limits.h was not included in your distribution, 
# please report it as a bug.  

# numeric.pl is not charset but is built with charset
charset:
	perl sybase_character_sets.h.pl > sybase_character_sets.h
	perl encodings.pl > encodings.h

## We have to build num_limits.h because automake can't infer the
## dependency (on numeric.pl).  By adding num_limits.h to libtds_la_SOURCES
## and/or libtds_objects_la_SOURCES, we cause  "make dist" to include it in
## the distribution tarball.

EXTRA_DIST =    num_limits.h

if HAVE_NUMERIC_PL
num_limits.h: numeric.pl
	perl $(srcdir)/numeric.pl > .$@ 
	mv .$@ $@
endif
