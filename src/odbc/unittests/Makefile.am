# $Id: Makefile.am,v 1.64 2007-04-20 09:15:06 freddy77 Exp $
TESTS		=	\
			t0001$(EXEEXT) t0002$(EXEEXT) t0003$(EXEEXT)\
			t0004$(EXEEXT) connect$(EXEEXT) print$(EXEEXT)\
			date$(EXEEXT) norowset$(EXEEXT) funccall$(EXEEXT)\
			lang_error$(EXEEXT) tables$(EXEEXT)\
			binary_test$(EXEEXT) moreandcount$(EXEEXT)\
			earlybind$(EXEEXT) putdata$(EXEEXT) params$(EXEEXT)\
			raiserror$(EXEEXT) getdata$(EXEEXT)\
			transaction$(EXEEXT) type$(EXEEXT) genparams$(EXEEXT)\
			preperror$(EXEEXT) prepare_results$(EXEEXT)\
			testodbc$(EXEEXT) data$(EXEEXT) error$(EXEEXT)\
			rebindpar$(EXEEXT) rpc$(EXEEXT) convert_error$(EXEEXT)\
			typeinfo$(EXEEXT) const_params$(EXEEXT)\
			insert_speed$(EXEEXT) compute$(EXEEXT)\
			timeout$(EXEEXT) array$(EXEEXT) array_out$(EXEEXT)\
			cursor1$(EXEEXT) scroll$(EXEEXT) cursor2$(EXEEXT)\
			describecol$(EXEEXT) copydesc$(EXEEXT)\
			prepclose$(EXEEXT) warning$(EXEEXT) \
			paramcore$(EXEEXT) timeout2$(EXEEXT) timeout3$(EXEEXT) \
			connect2$(EXEEXT) timeout4$(EXEEXT)

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
raiserror_SOURCES	= raiserror.c common.c common.h
getdata_SOURCES	= getdata.c common.c common.h
type_SOURCES	= type.c common.c common.h
transaction_SOURCES = transaction.c common.c common.h
genparams_SOURCES = genparams.c common.c common.h
preperror_SOURCES = preperror.c common.c common.h
prepare_results_SOURCES = prepare_results.c common.c common.h
testodbc_SOURCES	= testodbc.c common.c common.h
data_SOURCES	= data.c common.c common.h
error_SOURCES	= error.c common.c common.h
rebindpar_SOURCES	= rebindpar.c common.c common.h
rpc_SOURCES	= rpc.c common.c common.h
convert_error_SOURCES	= convert_error.c common.c common.h
typeinfo_SOURCES	= typeinfo.c common.c common.h
const_params_SOURCES	= const_params.c common.c common.h
insert_speed_SOURCES	= insert_speed.c common.c common.h
compute_SOURCES	= compute.c common.c common.h
timeout_SOURCES	= timeout.c common.c common.h
array_SOURCES	= array.c common.c common.h
array_out_SOURCES	= array_out.c common.c common.h
cursor1_SOURCES	= cursor1.c common.c common.h
scroll_SOURCES	= scroll.c common.c common.h
cursor2_SOURCES	= cursor2.c common.c common.h
describecol_SOURCES	= describecol.c common.c common.h
copydesc_SOURCES	= copydesc.c common.c common.h
prepclose_SOURCES = prepclose.c common.c common.h
warning_SOURCES = warning.c common.c common.h
paramcore_SOURCES = paramcore.c common.c common.h
timeout2_SOURCES	= timeout2.c common.c common.h
timeout3_SOURCES	= timeout3.c common.c common.h
connect2_SOURCES	=	connect2.c common.c common.h
timeout4_SOURCES	= timeout4.c common.c common.h

AM_CPPFLAGS	=	-I$(top_srcdir)/include $(ODBC_INC) -DFREETDS_SRCDIR=\"$(srcdir)\"
AM_LDFLAGS	=	-no-install
LIBS		=	$(ODBCLIB) $(ODBCNODMLIB) @NETWORK_LIBS@
DISTCLEANFILES	=	odbc.ini odbcinst.ini
EXTRA_DIST	=	describecol.in $(EXTRA_DIST_WIN32)

if HAVE_PERL_SOURCES
odbc.dsw: $(top_srcdir)/win32/build_dsw.pl Makefile
	rm -rf $@ odbc.mak vc6
	perl $(top_srcdir)/win32/build_dsw.pl $@ $(TESTS)

vc6: odbc.dsw

clean-local:
	rm -rf vc6

EXTRA_DIST_WIN32 = vc6 odbc.dsw odbc.mak
endif

