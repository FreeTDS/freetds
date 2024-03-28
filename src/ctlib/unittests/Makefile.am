NULL=
TESTS = \
	t0001$(EXEEXT) \
	t0002$(EXEEXT) \
	t0003$(EXEEXT) \
	t0004$(EXEEXT) \
	t0005$(EXEEXT) \
	cs_convert$(EXEEXT) \
	t0007$(EXEEXT) \
	t0008$(EXEEXT) \
	t0009$(EXEEXT) \
	connect_fail$(EXEEXT) \
	ct_options$(EXEEXT) \
	lang_ct_param$(EXEEXT) \
	array_bind$(EXEEXT) \
	cs_diag$(EXEEXT) \
	get_send_data$(EXEEXT) \
	rpc_ct_param$(EXEEXT) \
	rpc_ct_setparam$(EXEEXT) \
	ct_diagclient$(EXEEXT) \
	ct_diagserver$(EXEEXT) \
	ct_diagall$(EXEEXT) \
	cs_config$(EXEEXT) \
	cancel$(EXEEXT) \
	blk_in$(EXEEXT) \
	blk_out$(EXEEXT) \
	ct_cursor$(EXEEXT) \
	ct_cursors$(EXEEXT) \
	ct_dynamic$(EXEEXT) \
	blk_in2$(EXEEXT) \
	datafmt$(EXEEXT) \
	data$(EXEEXT) \
	rpc_fail$(EXEEXT) \
	row_count$(EXEEXT) \
	all_types$(EXEEXT) \
	long_binary$(EXEEXT) \
	will_convert$(EXEEXT) \
	variant$(EXEEXT) \
	errors$(EXEEXT) \
	ct_command$(EXEEXT) \
	$(NULL)

check_PROGRAMS	=	$(TESTS)

t0001_SOURCES		= t0001.c
t0002_SOURCES		= t0002.c
t0003_SOURCES		= t0003.c
t0004_SOURCES		= t0004.c
t0005_SOURCES		= t0005.c
cs_convert_SOURCES	= cs_convert.c
t0007_SOURCES		= t0007.c
t0008_SOURCES		= t0008.c
t0009_SOURCES		= t0009.c
connect_fail_SOURCES	= connect_fail.c
ct_options_SOURCES	= ct_options.c
lang_ct_param_SOURCES	= lang_ct_param.c
array_bind_SOURCES	= array_bind.c
cs_diag_SOURCES		= cs_diag.c
get_send_data_SOURCES	= get_send_data.c
rpc_ct_param_SOURCES	= rpc_ct_param.c
rpc_ct_setparam_SOURCES	= rpc_ct_setparam.c
ct_diagclient_SOURCES	= ct_diagclient.c
ct_diagserver_SOURCES	= ct_diagserver.c
ct_diagall_SOURCES	= ct_diagall.c
cs_config_SOURCES	= cs_config.c
cancel_SOURCES		= cancel.c
blk_in_SOURCES		= blk_in.h blk_in.c
blk_out_SOURCES		= blk_out.c
ct_cursor_SOURCES	= ct_cursor.c
ct_cursors_SOURCES	= ct_cursors.c
ct_dynamic_SOURCES	= ct_dynamic.c
blk_in2_SOURCES		= blk_in2.c
datafmt_SOURCES		= datafmt.c
data_SOURCES		= data.c
rpc_fail_SOURCES	= rpc_fail.c
row_count_SOURCES	= row_count.c
all_types_SOURCES	= all_types.c
all_types_LDFLAGS	= -static ../libct.la ../../tds/unittests/libcommon.a -shared
long_binary_SOURCES	= long_binary.c
will_convert_SOURCES	= will_convert.c
variant_SOURCES		= variant.c
errors_SOURCES		= errors.c
ct_command_SOURCES	= ct_command.c

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = common.c common.h

AM_CPPFLAGS	=	-I$(top_srcdir)/include
if MINGW32
AM_LDFLAGS	=	-no-fast-install
else
AM_LDFLAGS	=	-no-install -L../.libs -R "$(abs_builddir)/../.libs"
endif
LDADD		=	libcommon.a ../libct.la ../../replacements/libreplacements.la $(LTLIBICONV)
CLEANFILES	=	tdsdump.out
EXTRA_DIST	=	CMakeLists.txt
