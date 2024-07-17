TESTS =	\
	t0001$(EXEEXT) \
	t0002$(EXEEXT) \
	t0003$(EXEEXT) \
	t0004$(EXEEXT) \
	t0005$(EXEEXT) \
	t0006$(EXEEXT) \
	t0007$(EXEEXT) \
	t0008$(EXEEXT) \
	t0009$(EXEEXT) \
	t0011$(EXEEXT) \
	t0012$(EXEEXT) \
	t0013$(EXEEXT) \
	t0014$(EXEEXT) \
	t0015$(EXEEXT) \
	t0016$(EXEEXT) \
	t0017$(EXEEXT) \
	t0018$(EXEEXT) \
	t0019$(EXEEXT) \
	t0020$(EXEEXT) \
	dbsafestr$(EXEEXT) \
	t0022$(EXEEXT) \
	t0023$(EXEEXT) \
	rpc$(EXEEXT) \
	dbmorecmds$(EXEEXT) \
	bcp$(EXEEXT) \
	thread$(EXEEXT) \
	text_buffer$(EXEEXT) \
	done_handling$(EXEEXT) \
	timeout$(EXEEXT) \
	hang$(EXEEXT) \
	null$(EXEEXT) \
	null2$(EXEEXT) \
	setnull$(EXEEXT) \
	numeric$(EXEEXT) \
	pending$(EXEEXT) \
	cancel$(EXEEXT) \
	spid$(EXEEXT) \
	canquery$(EXEEXT) \
	batch_stmt_ins_sel$(EXEEXT) \
	batch_stmt_ins_upd$(EXEEXT) \
	bcp_getl$(EXEEXT) \
	empty_rowsets$(EXEEXT) \
	string_bind$(EXEEXT) \
	colinfo$(EXEEXT) \
	bcp2$(EXEEXT) \
	proc_limit$(EXEEXT)

check_PROGRAMS	=	$(TESTS)

t0001_SOURCES	=	t0001.c t0001.sql
t0002_SOURCES	=	t0002.c t0002.sql
t0003_SOURCES	=	t0003.c t0003.sql
t0004_SOURCES	=	t0004.c t0004.sql
t0005_SOURCES	=	t0005.c t0005.sql
t0006_SOURCES	=	t0006.c t0006.sql
t0007_SOURCES	=	t0007.c t0007.sql
t0008_SOURCES	=	t0008.c
t0009_SOURCES	=	t0009.c t0009.sql
t0011_SOURCES	=	t0011.c t0011.sql
t0012_SOURCES	=	t0012.c t0012.sql
t0013_SOURCES	=	t0013.c t0013.sql data.bin
t0014_SOURCES	=	t0014.c t0014.sql data.bin
t0015_SOURCES	=	t0015.c t0015.sql
t0016_SOURCES	=	t0016.c \
			t0016.sql t0016_1.sql t0016_2.sql t0016_3.sql \
			t0016_4.sql t0016_5.sql t0016_6.sql t0016_7.sql \
			t0016_8.sql t0016_9.sql t0016_10.sql t0016_11.sql \
			t0016.in t0016_1.in t0016_2.in t0016_3.in \
			t0016_4.in t0016_5.in t0016_6.in t0016_7.in \
			t0016_8.in t0016_9.in t0016_10.in t0016_11.in
t0017_SOURCES	=	t0017.c t0017.sql t0017.in t0017.in.be
t0018_SOURCES	=	t0018.c t0018.sql
t0019_SOURCES	=	t0019.c
t0020_SOURCES	=	t0020.c t0020.sql
dbsafestr_SOURCES =	dbsafestr.c
t0022_SOURCES	=	t0022.c t0022.sql
t0023_SOURCES	=	t0023.c t0023.sql
rpc_SOURCES	=	rpc.c rpc.sql
dbmorecmds_SOURCES =	dbmorecmds.c dbmorecmds.sql
bcp_SOURCES	=	bcp.c bcp.h bcp.sql
thread_SOURCES	=	thread.c bcp.h
text_buffer_SOURCES =	text_buffer.c text_buffer.sql
done_handling_SOURCES =	done_handling.c done_handling.sql
timeout_SOURCES =	timeout.c timeout.sql
hang_SOURCES	=	hang.c
null_SOURCES	=	null.c
null2_SOURCES	=	null2.c
setnull_SOURCES	=	setnull.c
numeric_SOURCES =	numeric.c numeric.sql numeric_2.sql
pending_SOURCES =	pending.c pending.sql
cancel_SOURCES =	cancel.c cancel.sql
spid_SOURCES	=	spid.c spid.sql
canquery_SOURCES=	canquery.c canquery.sql
batch_stmt_ins_sel_SOURCES	=	batch_stmt_ins_sel.c batch_stmt_ins_sel.sql
batch_stmt_ins_upd_SOURCES	=	batch_stmt_ins_upd.c batch_stmt_ins_upd.sql
bcp_getl_SOURCES=	bcp_getl.c
empty_rowsets_SOURCES = empty_rowsets.c empty_rowsets.sql
string_bind_SOURCES	=	string_bind.c
colinfo_SOURCES	=	colinfo.c colinfo.sql
bcp2_SOURCES	=	bcp2.c bcp2.sql
proc_limit_SOURCES	=	proc_limit.c

noinst_LIBRARIES = libcommon.a
libcommon_a_SOURCES = common.c common.h

AM_CPPFLAGS	= 	-DFREETDS_TOPDIR=\"$(top_srcdir)\" -I$(top_srcdir)/include
if FAST_INSTALL
AM_LDFLAGS	=	-no-fast-install
else
AM_LDFLAGS	=	-no-install -L../.libs -R "$(abs_builddir)/../.libs"
endif
LDADD		=	libcommon.a ../../utils/unittests/libtds_test_base.a \
			../libsybdb.la ../../replacements/libreplacements.la \
			$(LTLIBICONV)
EXTRA_DIST	=	CMakeLists.txt
CLEANFILES	=	tdsdump.out t0013.out t0014.out t0016.out \
				t0016.err t0017.err t0017.out
