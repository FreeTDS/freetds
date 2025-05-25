NULL =
noinst_HEADERS = \
	iconv.h \
	convert.h \
	bytes.h \
	thread.h \
	proto.h \
	tds.h \
	odbc.h \
	version.h \
	server.h \
	configs.h \
	enum_cap.h \
	sysdep_private.h \
	sysdep_types.h \
	stream.h \
	data.h \
	pushvis.h \
	popvis.h \
	time.h \
	tls.h \
	bool.h \
	checks.h \
	alloca.h \
	macros.h \
	windows.h \
	utils.h \
	utils/string.h \
	utils/dlist.h \
	utils/dlist.tmpl.h \
	utils/bjoern-utf8.h \
	utils/md4.h \
	utils/des.h \
	utils/md5.h \
	utils/hmac_md5.h \
	utils/smp.h \
	utils/path.h \
	utils/nosigpipe.h \
	utils/test_base.h \
	replacements.h \
	replacements/poll.h \
	replacements/readpassphrase.h \
	$(NULL)

DISTCLEANFILES = sysconfdir.h \
	sysdep_types.h \
	$(NULL)

sysconfdir.h: Makefile
	echo '#define FREETDS_SYSCONFDIR "$(sysconfdir)"' >$@

all-am: sysconfdir.h odbc_attributes_sorted

GENERATED_HEADER_FILES = encodings.h charset_lookup.h
EXTRA_DIST = $(GENERATED_HEADER_FILES)

if HAVE_PERL_SOURCES
BUILT_SOURCES = $(GENERATED_HEADER_FILES)

clean-local:
	cd $(srcdir) && rm -f $(BUILT_SOURCES)

charset_lookup.h: encodings.h

encodings.h: encodings.pl alternative_character_sets.h character_sets.h Makefile
	perl $(srcdir)/encodings.pl $(srcdir) $(GPERF) > $@.tmp 2> /dev/null
	mv $@.tmp $@

odbc_attributes_sorted: odbc_attributes.pl odbc.h Makefile
	perl $(srcdir)/odbc_attributes.pl $(srcdir)
	touch $@
else
odbc_attributes_sorted: odbc.h Makefile
	touch $@
endif
