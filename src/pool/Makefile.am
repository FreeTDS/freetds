bin_PROGRAMS = tdspool
tdspool_SOURCES	= config.c main.c member.c stream.c user.c util.c pool.h
SERVERDIR		=	../server
INCLUDES	=	-I$(top_srcdir)/include -I. -I$(SERVERDIR)
LDADD     =    ../tds/libtds.la ../server/libtdssrv.la
EXTRA_DIST = BUGS
