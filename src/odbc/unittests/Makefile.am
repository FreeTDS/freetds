TESTS		=	t0001 t0002
check_PROGRAMS	=	$(TESTS)

t0001_SOURCES	=	t0001.c common.c common.h
t0002_SOURCES	=	t0002.c common.c common.h

LIBS		=	-lodbc @NETWORK_LIBS@
INCLUDES	=	-I$(top_srcdir)/include -I$(ODBC_INC)
