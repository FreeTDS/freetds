TESTS		=	t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 \
			t0009 t0010 t0011 t0012 t0013 t0014 t0015 t0016 \
			t0017 t0018 t0019 t0020 t0021
EXTRA_PROGRAMS	=	t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 \
			t0009 t0010 t0011 t0012 t0013 t0014 t0015 t0016 \
			t0017 t0018 t0019 t0020 t0021

t0001_SOURCES	=	t0001.c common.c pwd.c
t0002_SOURCES	=	t0002.c common.c pwd.c
t0003_SOURCES	=	t0003.c common.c pwd.c
t0004_SOURCES	=	t0004.c common.c pwd.c
t0005_SOURCES	=	t0005.c common.c pwd.c
t0006_SOURCES	=	t0006.c common.c pwd.c
t0007_SOURCES	=	t0007.c common.c pwd.c
t0008_SOURCES	=	t0008.c common.c pwd.c
t0009_SOURCES	=	t0009.c common.c pwd.c
t0010_SOURCES	=	t0010.c common.c pwd.c
t0011_SOURCES	=	t0011.c common.c pwd.c
t0012_SOURCES	=	t0012.c common.c pwd.c
t0013_SOURCES	=	t0013.c common.c pwd.c
t0014_SOURCES	=	t0014.c common.c pwd.c
t0015_SOURCES	=	t0015.c common.c pwd.c
t0016_SOURCES	=	t0016.c common.c pwd.c
t0017_SOURCES	=	t0017.c common.c pwd.c
t0018_SOURCES	=	t0018.c common.c pwd.c
t0019_SOURCES	=	t0019.c common.c pwd.c
t0020_SOURCES	=	t0020.c common.c pwd.c
t0021_SOURCES	=	t0021.c 

LIBS		=	../libsybdb.la @NETWORK_LIBS@
INCLUDES	=	-I$(top_srcdir)/include
include_HEADERS =       common.h
EXTRA_DIST	=	t0016.in t0017.in data.bin

all-redirect:
	if test ! -f t0016.in; then cp $(srcdir)/t0016.in t0016.in; fi
	if test ! -f t0017.in; then cp $(srcdir)/t0017.in t0017.in; fi
	if test ! -f data.bin; then cp $(srcdir)/data.bin data.bin;  fi

