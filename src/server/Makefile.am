lib_LTLIBRARIES	=	libtdssrv.la
##EXTRA_LTLIBRARIES	=	libtdssrv.la
libtdssrv_la_SOURCES=	query.c server.c login.c server.h
libtdssrv_la_LDFLAGS=	-rpath $(libdir)
INCLUDES	=	-I$(top_srcdir)/include
EXTRA_DIST = unittest.c
## Need a blank statement to avoid compiling server.c
server: $(EXTRA_LTLIBRARIES)
	@echo ''

