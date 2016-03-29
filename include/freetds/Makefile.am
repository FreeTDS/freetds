
noinst_HEADERS = iconv.h string.h convert.h bytes.h \
	thread.h proto.h tds.h odbc.h version.h server.h \
	configs.h enum_cap.h sysdep_private.h stream.h \
	data.h pushvis.h popvis.h time.h tls.h bool.h \
	checks.h dlist.h dlist.tmpl.h alloca.h

DISTCLEANFILES = sysconfdir.h

sysconfdir.h: Makefile
	echo '#define FREETDS_SYSCONFDIR "$(sysconfdir)"' >$@

all-am: sysconfdir.h
