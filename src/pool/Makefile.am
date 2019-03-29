AM_CPPFLAGS	=	-I$(top_srcdir)/include -I. -I$(SERVERDIR)
bin_PROGRAMS	=	tdspool

tdspool_SOURCES	=	config.c main.c member.c user.c util.c pool.h
SERVERDIR	=	../server
LDADD		=	../server/libtdssrv.la $(LTLIBICONV)
EXTRA_DIST	=	BUGS pool.conf CMakeLists.txt

ETC	=	$(DESTDIR)$(sysconfdir)

install-data-local:
	$(mkinstalldirs) $(ETC)
	if test ! -f $(ETC)/pool.conf; then \
		$(INSTALL) -m 644 $(srcdir)/pool.conf $(ETC)/pool.conf; \
	fi
