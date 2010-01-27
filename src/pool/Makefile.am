# $Id: Makefile.am,v 1.15 2010-01-27 03:21:08 jklowden Exp $
AM_CPPFLAGS	=	-I$(top_srcdir)/include -I. -I$(SERVERDIR)
bin_PROGRAMS	=	tdspool

tdspool_SOURCES	=	config.c main.c member.c user.c util.c pool.h
SERVERDIR	=	../server
LDADD		=	../server/libtdssrv.la $(LIBICONV) $(NETWORK_LIBS)
EXTRA_DIST	=	BUGS pool.conf

ETC	=	$(DESTDIR)$(sysconfdir)

install-data-local:
	$(mkinstalldirs) $(ETC)
	if test ! -f $(ETC)/pool.conf; then \
		$(INSTALL_DATA) $(srcdir)/pool.conf $(ETC)/pool.conf; \
	fi
