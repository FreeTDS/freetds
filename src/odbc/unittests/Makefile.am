# $Id: Makefile.am,v 1.28 2004-02-09 13:04:05 freddy77 Exp $
TESTS		=	\
			t0001 t0002 t0003 t0004 connect print date \
			norowset funccall lang_error tables \
			binary_test moreandcount earlybind \
			putdata params raiseerror getdata \
			transaction \
			type genparams preperror \
			prepare_results

check_PROGRAMS	=	$(TESTS)

t0001_SOURCES	=	t0001.c common.c common.h
t0002_SOURCES	=	t0002.c common.c common.h
t0003_SOURCES	=	t0003.c common.c common.h
t0004_SOURCES	=	t0004.c common.c common.h
connect_SOURCES	=	connect.c common.c common.h
print_SOURCES	=	print.c common.c common.h
date_SOURCES	=	date.c common.c common.h
norowset_SOURCES	= norowset.c common.c common.h
funccall_SOURCES	= funccall.c common.c common.h
lang_error_SOURCES	= lang_error.c common.c common.h
tables_SOURCES	= tables.c common.c common.h
binary_test_SOURCES	= binary_test.c common.c common.h
moreandcount_SOURCES	= moreandcount.c common.c common.h
earlybind_SOURCES	= earlybind.c common.c common.h
putdata_SOURCES	= putdata.c common.c common.h
params_SOURCES	= params.c common.c common.h
raiseerror_SOURCES	= raiseerror.c common.c common.h
getdata_SOURCES	= getdata.c common.c common.h
type_SOURCES	= type.c common.c common.h
transaction_SOURCES = transaction.c common.c common.h
genparams_SOURCES = genparams.c common.c common.h
preperror_SOURCES = preperror.c common.c common.h
prepare_results_SOURCES = prepare_results.c common.c common.h

LIBS		=	$(ODBCLIB) $(ODBCNODMLIB) @NETWORK_LIBS@
INCLUDES	=	-I$(top_srcdir)/include $(ODBC_INC)
DISTCLEANFILES	=	myodbc.ini odbcinst.ini

