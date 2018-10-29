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
	$(NULL)

DISTCLEANFILES = sysconfdir.h

sysconfdir.h: Makefile
	echo '#define FREETDS_SYSCONFDIR "$(sysconfdir)"' >$@

all-am: sysconfdir.h
