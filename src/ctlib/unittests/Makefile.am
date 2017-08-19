TESTS		=	t0001$(EXEEXT) t0002$(EXEEXT) t0003$(EXEEXT) t0004$(EXEEXT) \
			t0005$(EXEEXT) t0006$(EXEEXT) t0007$(EXEEXT) t0008$(EXEEXT) \
			t0009$(EXEEXT) connect_fail$(EXEEXT) ct_options$(EXEEXT) \
			lang_ct_param$(EXEEXT) array_bind$(EXEEXT) cs_diag$(EXEEXT) \
			get_send_data$(EXEEXT) rpc_ct_param$(EXEEXT) rpc_ct_setparam$(EXEEXT) \
			ct_diagclient$(EXEEXT) ct_diagserver$(EXEEXT) ct_diagall$(EXEEXT) \
			cs_config$(EXEEXT) cancel$(EXEEXT) blk_in$(EXEEXT) \
			blk_out$(EXEEXT) ct_cursor$(EXEEXT) ct_cursors$(EXEEXT) \
			ct_dynamic$(EXEEXT) blk_in2$(EXEEXT) datafmt$(EXEEXT) \
			rpc_fail$(EXEEXT) row_count$(EXEEXT)

check_PROGRAMS	=	$(TESTS)

t0001_SOURCES		= t0001.c
t0002_SOURCES		= t0002.c
t0003_SOURCES		= t0003.c
t0004_SOURCES		= t0004.c
t0005_SOURCES		= t0005.c
t0006_SOURCES		= t0006.c
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
rpc_fail_SOURCES	= rpc_fail.c
row_count_SOURCES	= row_count.c

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = common.c common.h
LDADD = libcommon.a

AM_CPPFLAGS	=	-I$(top_srcdir)/include
if MINGW32
AM_LDFLAGS	=	-no-fast-install
else
AM_LDFLAGS	=	-no-install -L../.libs -R $(abs_builddir)/../.libs
endif
LIBS		=	../libct.la ../../replacements/libreplacements.la $(LTLIBICONV)
CLEANFILES	=	tdsdump.out
EXTRA_DIST	=	CMakeLists.txt
