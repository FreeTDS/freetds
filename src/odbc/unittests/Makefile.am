TESTS		=	t0001 t0002 t0003 t0004 connect print date
check_PROGRAMS	=	$(TESTS)

t0001_SOURCES	=	t0001.c common.c common.h
t0002_SOURCES	=	t0002.c common.c common.h
t0003_SOURCES	=	t0003.c common.c common.h
t0004_SOURCES	=	t0004.c common.c common.h
connect_SOURCES	=	connect.c common.c common.h
print_SOURCES	=	print.c common.c common.h
date_SOURCES	=	date.c common.c common.h

LIBS		=	-lodbc @NETWORK_LIBS@
INCLUDES	=	-I$(top_srcdir)/include -I$(ODBC_INC)
