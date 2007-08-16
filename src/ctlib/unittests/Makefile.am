# $Id: Makefile.am,v 1.25 2007-08-16 08:10:09 freddy77 Exp $
TESTS		=	t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 t0009 \
			connect_fail ct_options lang_ct_param array_bind cs_diag \
			get_send_data rpc_ct_param rpc_ct_setparam ct_diagclient \
			ct_diagserver ct_diagall cs_config cancel \
			blk_in blk_out ct_cursor ct_cursors \
			ct_dynamic blk_in2
check_PROGRAMS	=	$(TESTS)

t0001_SOURCES		= t0001.c common.c common.h
t0002_SOURCES		= t0002.c common.c common.h
t0003_SOURCES		= t0003.c common.c common.h
t0004_SOURCES		= t0004.c common.c common.h
t0005_SOURCES		= t0005.c common.c common.h
t0006_SOURCES		= t0006.c
t0007_SOURCES		= t0007.c common.c common.h
t0008_SOURCES		= t0008.c common.c common.h
t0009_SOURCES		= t0009.c common.c common.h
connect_fail_SOURCES	= connect_fail.c common.c common.h
ct_options_SOURCES	= ct_options.c common.c common.h
lang_ct_param_SOURCES	= lang_ct_param.c common.c common.h
array_bind_SOURCES	= array_bind.c common.c common.h
cs_diag_SOURCES		= cs_diag.c common.c common.h
get_send_data_SOURCES	= get_send_data.c common.c common.h
rpc_ct_param_SOURCES	= rpc_ct_param.c common.c common.h
rpc_ct_setparam_SOURCES	= rpc_ct_setparam.c common.c common.h
ct_diagclient_SOURCES	= ct_diagclient.c common.c common.h
ct_diagserver_SOURCES	= ct_diagserver.c common.c common.h
ct_diagall_SOURCES	= ct_diagall.c common.c common.h
cs_config_SOURCES	= cs_config.c common.c common.h
cancel_SOURCES		= cancel.c common.c common.h
blk_in_SOURCES		= blk_in.h blk_in.c common.c common.h
blk_out_SOURCES		= blk_out.c common.c common.h
ct_cursor_SOURCES	= ct_cursor.c common.c common.h
ct_cursors_SOURCES	= ct_cursors.c common.c common.h
ct_dynamic_SOURCES	= ct_dynamic.c common.c common.h
blk_in2_SOURCES		= blk_in2.c common.c common.h

AM_CPPFLAGS	=	-I$(top_srcdir)/include
if MINGW32
AM_LDFLAGS	=	-no-fast-install
else
AM_LDFLAGS	=	-no-install
endif
LIBS		=	../libct.la @NETWORK_LIBS@
CLEANFILES	=	tdsdump.out
