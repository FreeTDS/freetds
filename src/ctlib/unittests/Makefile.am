TESTS		=	t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 t0009 \
			connect_fail
check_PROGRAMS	=	$(TESTS)

t0001_SOURCES	=	t0001.c common.c common.h
t0002_SOURCES	=	t0002.c common.c common.h
t0003_SOURCES	=	t0003.c common.c common.h
t0004_SOURCES	=	t0004.c common.c common.h
t0005_SOURCES	=	t0005.c common.c common.h
t0006_SOURCES	=	t0006.c
t0007_SOURCES	=	t0007.c common.c common.h
t0008_SOURCES	=	t0008.c common.c common.h
t0009_SOURCES	=	t0009.c common.c common.h
connect_fail_SOURCES	= connect_fail.c common.c common.h

LIBS		=	../libct.la @NETWORK_LIBS@
INCLUDES	=	-I$(top_srcdir)/include
CLEANFILES	=	tdsdump.out
