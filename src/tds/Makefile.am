# $Id: Makefile.am,v 1.35 2004-03-22 19:13:13 freddy77 Exp $

SUBDIRS			=	unittests

INCLUDES		=	-I$(top_srcdir)/include
noinst_LTLIBRARIES	=	libtds_objects.la
libtds_objects_la_SOURCES=	mem.c token.c util.c login.c read.c \
	write.c convert.c numeric.c config.c query.c iconv.c \
	locale.c challenge.c threadsafe.c vstrbuild.c md4.c md5.c \
	des.c tdsstring.c getmac.c data.c net.c
libtds_objects_la_LDFLAGS=
libtds_objects_la_LIBADD=

lib_LTLIBRARIES		=	libtds.la
libtds_la_SOURCES	=
libtds_la_LDFLAGS	= -version-info 3:0:0
libtds_la_LIBADD 	= libtds_objects.la $(NETWORK_LIBS) $(LIBICONV) ../replacements/libreplacements.la
noinst_HEADERS		= tds_willconvert.h character_sets.h \
			  alternative_character_sets.h sybase_character_sets.h \
			  encodings.h

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

charset:
	perl sybase_character_sets.h.pl > sybase_character_sets.h
	perl encodings.pl > encodings.h

