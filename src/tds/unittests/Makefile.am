# $Id: Makefile.am,v 1.23.2.1 2010-07-31 08:05:20 freddy77 Exp $
TESTS		=	t0001$(EXEEXT) t0002$(EXEEXT) t0003$(EXEEXT)\
			t0004$(EXEEXT) t0005$(EXEEXT) t0006$(EXEEXT)\
			t0007$(EXEEXT) t0008$(EXEEXT) dynamic1$(EXEEXT)\
			convert$(EXEEXT) dataread$(EXEEXT) utf8_1$(EXEEXT)\
			utf8_2$(EXEEXT) utf8_3$(EXEEXT) numeric$(EXEEXT) \
			iconv_fread$(EXEEXT)

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
iconv_fread_SOURCES	= iconv_fread.c

AM_CPPFLAGS	=	-I$(top_srcdir)/include -I$(srcdir)/.. -I../
if MINGW32
AM_LDFLAGS	=	-no-fast-install
else
AM_LDFLAGS	=	-no-install
endif
LIBS		=	../libtds.la ../../replacements/libreplacements.la $(LIBICONV) @NETWORK_LIBS@
CLEANFILES	=	tdsdump.out
