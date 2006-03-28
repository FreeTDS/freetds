# $Id: Makefile.am,v 1.18 2006-03-28 00:05:57 jklowden Exp $
TESTS		=	t0001 t0002 t0003 t0004 \
			t0005 t0006 t0007 t0008 \
			dynamic1 convert dataread \
			utf8_1 utf8_2 utf8_3 \
			numeric

# flags test commented, not necessary for 0.62
# TODO add flags test again when needed

check_PROGRAMS	=	$(TESTS)

t0001_SOURCES	=	t0001.c common.c common.h
t0002_SOURCES	=	t0002.c common.c common.h
t0003_SOURCES	=	t0003.c common.c common.h
t0004_SOURCES	=	t0004.c common.c common.h
t0005_SOURCES	=	t0005.c common.c common.h
t0006_SOURCES	=	t0006.c common.c common.h
t0007_SOURCES	=	t0007.c
t0008_SOURCES	=	t0008.c
dynamic1_SOURCES =	dynamic1.c common.c common.h
convert_SOURCES	=	convert.c
dataread_SOURCES	=	dataread.c common.c common.h
# flags_SOURCES	=	flags.c common.c common.h
utf8_1_SOURCES	=	utf8_1.c common.c common.h
utf8_2_SOURCES	=	utf8_2.c common.c common.h
utf8_3_SOURCES	=	utf8_3.c common.c common.h
numeric_SOURCES =       numeric.c

AM_CPPFLAGS	=	-I$(top_srcdir)/include -I$(srcdir)/..
AM_LDFLAGS	=	-no-install
LIBS		=	../libtds.la @NETWORK_LIBS@
CLEANFILES	=	tdsdump.out
