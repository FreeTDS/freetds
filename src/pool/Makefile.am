# $Id: Makefile.am,v 1.12 2004-02-03 17:50:10 jklowden Exp $
bin_PROGRAMS	=	tdspool
tdspool_SOURCES	=	config.c main.c member.c stream.c user.c util.c pool.h
SERVERDIR	=	../server
INCLUDES	=	-I$(top_srcdir)/include -I. -I$(SERVERDIR)
LDADD		=	../server/libtdssrv.la $(NETWORK_LIBS)
EXTRA_DIST	=	BUGS pool.conf

ETC	=	$(DESTDIR)$(sysconfdir)

install-data-local:
	$(mkinstalldirs) $(ETC)
	if test ! -f $(ETC)/pool.conf; then \
		$(INSTALL_DATA) $(srcdir)/pool.conf $(ETC)/pool.conf; \
	fi
