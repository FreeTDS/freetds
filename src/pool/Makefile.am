bin_PROGRAMS = tdspool
tdspool_SOURCES	= config.c main.c member.c stream.c user.c util.c pool.h
TDSDIR		=	../tds
TDSSOURCES	=	mem.c token.c util.c login.c read.c write.c convert.c numeric.c config.c query.c iconv.c
SERVERDIR		=	../server
SERVERSOURCES	=	login.c query.c server.c
INCLUDES	=	-I$(top_srcdir)/include -I. -I$(SERVERDIR)
LDADD     =    ../tds/libtds.la ../server/libtdssrv.la
EXTRA_DIST = BUGS

