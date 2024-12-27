NULL=
TESTS =	\
	t0001$(EXEEXT) \
	t0002$(EXEEXT) \
	t0003$(EXEEXT) \
	moreresults$(EXEEXT) \
	connect$(EXEEXT) \
	print$(EXEEXT) \
	date$(EXEEXT) \
	norowset$(EXEEXT) \
	funccall$(EXEEXT) \
	lang_error$(EXEEXT) \
	tables$(EXEEXT) \
	binary_test$(EXEEXT) \
	moreandcount$(EXEEXT) \
	earlybind$(EXEEXT) \
	putdata$(EXEEXT) \
	params$(EXEEXT) \
	raiserror$(EXEEXT) \
	getdata$(EXEEXT) \
	transaction$(EXEEXT) \
	type$(EXEEXT) \
	genparams$(EXEEXT) \
	preperror$(EXEEXT) \
	prepare_results$(EXEEXT) \
	testodbc$(EXEEXT) \
	data$(EXEEXT) \
	error$(EXEEXT) \
	rebindpar$(EXEEXT) \
	rpc$(EXEEXT) \
	convert_error$(EXEEXT) \
	typeinfo$(EXEEXT) \
	const_params$(EXEEXT) \
	insert_speed$(EXEEXT) \
	compute$(EXEEXT) \
	timeout$(EXEEXT) \
	array$(EXEEXT) \
	array_out$(EXEEXT) \
	cursor1$(EXEEXT) \
	scroll$(EXEEXT) \
	cursor2$(EXEEXT) \
	describecol$(EXEEXT) \
	copydesc$(EXEEXT) \
	prepclose$(EXEEXT) \
	warning$(EXEEXT) \
	paramcore$(EXEEXT) \
	timeout2$(EXEEXT) \
	timeout3$(EXEEXT) \
	connect2$(EXEEXT) \
	timeout4$(EXEEXT) \
	freeclose$(EXEEXT) \
	cursor3$(EXEEXT) \
	cursor4$(EXEEXT) \
	cursor5$(EXEEXT) \
	attributes$(EXEEXT) \
	hidden$(EXEEXT) \
	blob1$(EXEEXT) \
	cancel$(EXEEXT) \
	wchar$(EXEEXT) \
	rowset$(EXEEXT) \
	transaction2$(EXEEXT) \
	cursor6$(EXEEXT) \
	cursor7$(EXEEXT) \
	utf8$(EXEEXT) \
	utf8_2$(EXEEXT) \
	utf8_3$(EXEEXT) \
	stats$(EXEEXT) \
	descrec$(EXEEXT) \
	peter$(EXEEXT) \
	test64$(EXEEXT) \
	prepare_warn$(EXEEXT) \
	long_error$(EXEEXT) \
	mars1$(EXEEXT) \
	array_error$(EXEEXT) \
	describecol2$(EXEEXT) \
	closestmt$(EXEEXT) \
	bcp$(EXEEXT) \
	all_types$(EXEEXT) \
	empty_query$(EXEEXT) \
	transaction3$(EXEEXT) \
	transaction4$(EXEEXT) \
	utf8_4$(EXEEXT) \
	qn$(EXEEXT) \
	connection_string_parse$(EXEEXT) \
	tvp$(EXEEXT) \
	tokens$(EXEEXT) \
	$(NULL)

check_PROGRAMS	=	$(TESTS) oldpwd$(EXEEXT)

t0001_SOURCES	=	t0001.c
t0002_SOURCES	=	t0002.c
t0003_SOURCES	=	t0003.c
moreresults_SOURCES	=	moreresults.c
connect_SOURCES	=	connect.c
connect_LDFLAGS = $(GLOBAL_LD_ADD) -no-install
print_SOURCES	=	print.c
date_SOURCES	=	date.c
norowset_SOURCES	= norowset.c
funccall_SOURCES	= funccall.c
lang_error_SOURCES	= lang_error.c
tables_SOURCES	= tables.c
binary_test_SOURCES	= binary_test.c
moreandcount_SOURCES	= moreandcount.c
earlybind_SOURCES	= earlybind.c
putdata_SOURCES	= putdata.c
params_SOURCES	= params.c
raiserror_SOURCES	= raiserror.c
getdata_SOURCES	= getdata.c
type_SOURCES	= type.c
transaction_SOURCES = transaction.c
genparams_SOURCES = genparams.c
preperror_SOURCES = preperror.c
prepare_results_SOURCES = prepare_results.c
testodbc_SOURCES	= testodbc.c
data_SOURCES	= data.c data.in
error_SOURCES	= error.c
rebindpar_SOURCES	= rebindpar.c
rpc_SOURCES	= rpc.c
convert_error_SOURCES	= convert_error.c
typeinfo_SOURCES	= typeinfo.c
const_params_SOURCES	= const_params.c
insert_speed_SOURCES	= insert_speed.c
compute_SOURCES	= compute.c
timeout_SOURCES	= timeout.c
array_SOURCES	= array.c
array_out_SOURCES	= array_out.c
cursor1_SOURCES	= cursor1.c
scroll_SOURCES	= scroll.c
cursor2_SOURCES	= cursor2.c
describecol_SOURCES	= describecol.c describecol.in
describecol2_SOURCES	= describecol2.c
copydesc_SOURCES	= copydesc.c
prepclose_SOURCES = prepclose.c
warning_SOURCES = warning.c
paramcore_SOURCES = paramcore.c
timeout2_SOURCES	= timeout2.c
timeout3_SOURCES	= timeout3.c
connect2_SOURCES	=	connect2.c
timeout4_SOURCES	= timeout4.c
freeclose_SOURCES	= freeclose.c
cursor3_SOURCES	= cursor3.c
cursor4_SOURCES	= cursor4.c
cursor5_SOURCES	= cursor5.c
attributes_SOURCES	= attributes.c attributes.in
hidden_SOURCES	= hidden.c
blob1_SOURCES	= blob1.c
rowset_SOURCES	= rowset.c
cancel_SOURCES = cancel.c
wchar_SOURCES = wchar.c
transaction2_SOURCES = transaction2.c
cursor6_SOURCES	= cursor6.c
cursor7_SOURCES	= cursor7.c
utf8_SOURCES	= utf8.c common.c
# this test cannot work using wide characters as use UTF-8 and single byte encoding
utf8_CPPFLAGS	=	$(GLOBAL_CPPFLAGS)
utf8_LDADD	=	$(ODBC_LDFLAGS) $(NETWORK_LIBS) $(ODBCINST_LDFLAGS)
utf8_2_SOURCES	= utf8_2.c
utf8_3_SOURCES	= utf8_3.c
utf8_4_SOURCES	= utf8_4.c
utf8_4_LDFLAGS = -static ../libtdsodbc.la ../../tds/unittests/libcommon.a -shared $(GLOBAL_LD_ADD)
stats_SOURCES	= stats.c
descrec_SOURCES	= descrec.c
peter_SOURCES	= peter.c
test64_SOURCES = test64.c
prepare_warn_SOURCES = prepare_warn.c
long_error_SOURCES = long_error.c
mars1_SOURCES = mars1.c
array_error_SOURCES = array_error.c
closestmt_SOURCES = closestmt.c
oldpwd_SOURCES = oldpwd.c
# this test cannot work using wide characters as use UTF-8 and single byte encoding
oldpwd_CPPFLAGS	=	$(GLOBAL_CPPFLAGS)
bcp_SOURCES = bcp.c
all_types_SOURCES = all_types.c
all_types_LDFLAGS = -static ../libtdsodbc.la ../../tds/unittests/libcommon.a -shared $(GLOBAL_LD_ADD)
empty_query_SOURCES = empty_query.c
transaction3_SOURCES = transaction3.c
transaction4_SOURCES = transaction4.c
qn_SOURCES = qn.c
connection_string_parse_SOURCES = connection_string_parse.c
connection_string_parse_CPPFLAGS = $(GLOBAL_CPPFLAGS)
connection_string_parse_LDFLAGS = -static ../libtdsodbc.la ../../tds/unittests/libcommon.a -shared $(GLOBAL_LD_ADD)
tokens_SOURCES	= tokens.c
tokens_LDADD = libcommon.a $(ODBC_LDFLAGS) ../../replacements/libreplacements.la ../../server/libtdssrv.la $(GLOBAL_LD_ADD)

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = common.c common.h c2string.c parser.c parser.h \
	fake_thread.c fake_thread.h

GLOBAL_CPPFLAGS	=	-I$(top_srcdir)/include $(ODBC_INC) -DFREETDS_TOPDIR=\"$(top_srcdir)\"
GLOBAL_LD_ADD =	$(NETWORK_LIBS) $(ODBCINST_LDFLAGS)
AM_CPPFLAGS	=	$(GLOBAL_CPPFLAGS)
if ODBC_WIDE_TESTS
AM_CPPFLAGS	+=	-DUNICODE -D_UNICODE
endif

if MINGW32
AM_LDFLAGS	=	-no-fast-install
else
AM_LDFLAGS	=	-no-install -L../.libs -R "$(abs_builddir)/../.libs"
endif
LDADD		=	libcommon.a $(ODBC_LDFLAGS) ../../replacements/libreplacements.la $(GLOBAL_LD_ADD)
DISTCLEANFILES	=	odbc.ini odbcinst.ini
EXTRA_DIST	=	CMakeLists.txt

