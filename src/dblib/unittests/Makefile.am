TESTS		=	t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 \
			t0009 t0010 t0011 t0012 t0013 t0014 t0015 t0016 \
			t0017 t0018 t0019 t0020 t0021 t0022 t0023 rpc
check_PROGRAMS	=	t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 \
			t0009 t0010 t0011 t0012 t0013 t0014 t0015 t0016 \
			t0017 t0018 t0019 t0020 t0021 t0022 t0023 rpc

t0001_SOURCES	=	t0001.c common.c common.h
t0002_SOURCES	=	t0002.c common.c common.h
t0003_SOURCES	=	t0003.c common.c common.h
t0004_SOURCES	=	t0004.c common.c common.h
t0005_SOURCES	=	t0005.c common.c common.h
t0006_SOURCES	=	t0006.c common.c common.h
t0007_SOURCES	=	t0007.c common.c common.h
t0008_SOURCES	=	t0008.c common.c common.h
t0009_SOURCES	=	t0009.c common.c common.h
t0010_SOURCES	=	t0010.c common.c common.h
t0011_SOURCES	=	t0011.c common.c common.h
t0012_SOURCES	=	t0012.c common.c common.h
t0013_SOURCES	=	t0013.c common.c common.h
t0014_SOURCES	=	t0014.c common.c common.h
t0015_SOURCES	=	t0015.c common.c common.h
t0016_SOURCES	=	t0016.c common.c common.h
t0017_SOURCES	=	t0017.c common.c common.h
t0018_SOURCES	=	t0018.c common.c common.h
t0019_SOURCES	=	t0019.c common.c common.h
t0020_SOURCES	=	t0020.c common.c common.h
t0021_SOURCES	=	t0021.c common.c common.h
t0022_SOURCES	=	t0022.c common.c common.h
t0023_SOURCES	=	t0023.c common.c common.h
rpc_SOURCES	=	rpc.c common.c common.h

LIBS		=	../libsybdb.la @NETWORK_LIBS@
INCLUDES	=	-I$(top_srcdir)/include
EXTRA_DIST	=	t0016.in t0017.in data.bin
CLEANFILES	=	tdsdump.out t0013.out t0014.out t0016.out \
	t0016.err t0017.err
AM_CPPFLAGS	= 	-DFREETDS_SRCDIR=\"$(srcdir)\"
