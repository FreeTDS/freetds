# FreeTDS - Library of routines accessing Sybase and Microsoft databases
# Copyright (C) 2003  Craig A. Berry   craigberry@mac.com      23-JAN-2003
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
# 

# OpenVMS description file for FreeTDS

# To build with ODBC support do MM(K|S)/MACRO="ODBC"=1
# This presupposes the existence of an ODBC library in the location pointed to
# by the logical name ODBC_LIBDIR and ODBC include files in the location pointed
# to by ODBC_INCDIR
#
# To build in debug, do MM(K|S)/MACRO="__DEBUG__"=1
#
# To build with MSDBLIB-compatible dblib structures, do MM(K|S)/MACRO="MSDBLIB"=1
#
# Other notes:
#  - CC/INCLUDE paths need to be Unix-style path, otherwise a #include
#    directive inside a header will fail if that directive uses Unix-style

OBJ = .OBJ
E = .EXE
OLB = .OLB

.SUFFIXES :
.SUFFIXES : $(E) $(OLB) $(OBJ) .C .H

TTDIR = [.src.tds.unittests]
CTDIR = [.src.ctlib.unittests]
DTDIR = [.src.dblib.unittests]
OTDIR = [.src.odbc.unittests]
UTDIR = [.src.utils.unittests]


all : prelim _all
	@ continue

prelim :
	@ type user.mms

.IFDEF ODBC
ODBC_INC=,"./src/odbc",ODBC_INCDIR
TDSODBCSHR=[]libtdsodbc$(E)
TDSODBCCHECK=TDSODBCCHECK
ODBCTESTS=ODBCTESTS
.ELSE
ODBC_INC=
TDSODBCSHR=
TDSODBCCHECK=
ODBCTESTS=
.ENDIF

.IFDEF SYBASE_COMPAT
DBOPENOBJ = [.src.dblib]dbopen$(OBJ), 
.ELSE
DBOPENOBJ =
.ENDIF

.IFDEF MSDBLIB
DBLIB_DEFINE = define MSDBLIB 1
.ELSE
DBLIB_DEFINE = define SYBDBLIB 1
.ENDIF

CC = CC/DECC

.IF $(ENABLE_THREAD_SAFE) .EQ 1
PTHREAD_CDEFINE = "_THREAD_SAFE"=1
PTHREAD_LINK_FLAGS = /THREADS=UPCALLS
.ELSE
PTHREAD_CDEFINE =
PTHREAD_LINK_FLAGS =
.ENDIF

.IF $(D_OPENSSL) .EQ 1
OPENSSL_OPTIONS = ,[]openssl.opt/OPTIONS
.ELSE
OPENSSL_OPTIONS =
.ENDIF

.IF $(D_STDINT) .EQ 1
STDINT_H = [.vms]discard.tmp
.ELSE
STDINT_H = [.include]stdint.h
.ENDIF

CDEFINE =

.IFDEF ODBC_WIDE
ODBC_WIDE_DEFINE = ,"ENABLE_ODBC_WIDE"
.ENDIF

.IFDEF ODBC_MARS
ODBC_MARS_DEFINE = ,"ENABLE_ODBC_MARS"
.ENDIF

.IFDEF ODBC
ODBC_CDEFINE = "UNIXODBC"$(ODBC_WIDE_DEFINE)$(ODBC_MARS_DEFINE)
CODBCFLAGS = /NAMES=(AS_IS,SHORTENED)
.ELSE
ODBC_CDEFINE =
CODBCFLAGS = /NAMES=SHORTENED
.ENDIF

.IFDEF ODBC_CDEFINE
.IFDEF CDEFINE
CDEFINE = $(CDEFINE),$(ODBC_CDEFINE)
.ELSE
CDEFINE = $(ODBC_CDEFINE)
.ENDIF
.ENDIF

.IFDEF PTHREAD_CDEFINE
.IFDEF CDEFINE
CDEFINE = $(CDEFINE),$(PTHREAD_CDEFINE)
.ELSE
CDEFINE = $(PTHREAD_CDEFINE)
.ENDIF
.ENDIF

.IFDEF CDEFINE
CDEFINE_QUAL = /DEFINE=($(CDEFINE))
.ELSE
CDEFINE_QUAL =
.ENDIF

CPREFIX = ALL
CINCLUDE = "./include"$(ODBC_INC)

.IFDEF __DEBUG__
CDBGFLAGS = /DEBUG/NOOPTIMIZE/LIST=$(MMS$TARGET_NAME)/SHOW=(EXPANSION,INCLUDE)
LDBGFLAGS = /DEBUG/MAP
.ELSE
CDBGFLAGS =
LDBGFLAGS = /NOTRACE
.ENDIF

CFLAGS = $(CDEFINE_QUAL)/PREFIX=($(CPREFIX))/MAIN=POSIX_EXIT/FLOAT=IEEE/IEEE=DENORM/OBJECT=$(MMS$TARGET_NAME)$(OBJ) $(CODBCFLAGS) $(CDBGFLAGS)
LINKFLAGS = $(LDBGFLAGS)$(PTHREAD_LINK_FLAGS)

CC_COMMAND = $(CC) $(CFLAGS)/INCLUDE=(${LOCALINCLUDE}$(CINCLUDE)) $(CDBGFLAGS) $(MMS$SOURCE)
.c$(OBJ) :
	$(CC_COMMAND)

$(OBJ)$(OLB) :
	@ IF F$SEARCH("$(MMS$TARGET)") .EQS. "" -
		THEN LIBRARY/CREATE/LOG $(MMS$TARGET)
	@ LIBRARY /REPLACE /LOG $(MMS$TARGET) $(MMS$SOURCE)

# This rule must exist in order for MMK dir-specific rule to override it,
$(OBJ)$(EXE) :
	dummy

# Objects that are only included if configure.com detected they were needed
# (Several of these are always present on OpenVMS versions still in support)
.IF $(D_ASPRINTF) .NE 1
ASPRINTFOBJ = [.src.replacements]asprintf$(OBJ)
.ENDIF
.IF $(D_VASPRINTF) .NE 1
VASPRINTFOBJ = [.src.replacements]vasprintf$(OBJ)
.ENDIF
.IF $(D_STRTOK_R) .NE 1
STRTOK_ROBJ = [.src.replacements]strtok_r$(OBJ)
.ENDIF
.IF $(D_ICONV) .NE 1
LIBICONVOBJ = [.src.replacements]libiconv$(OBJ)
$(LIBICONVOBJ) : [.src.replacements]libiconv.c
.ENDIF
.IF $(D_SNPRINTF) .NE 1
SNPRINTFOBJ = [.src.replacements]snprintf$(OBJ)
.ENDIF
.IF $(D_SOCKETPAIR) .NE 1
SOCKETPAIROBJ = [.src.replacements]socketpair$(OBJ)
.ENDIF

maintainer-clean-extra : maintainer-clean
	#! Files needed to manually import, since we don't have VMS gperf
	@[.vms]clean_delete [.include.freetds]encodings.h;*
	@[.vms]clean_delete [.include.freetds]charset_lookup.h;*

maintainer-clean : distclean
	#! Files created by configure_trunk.com
	@[.vms]clean_delete [.include.freetds]version.h;*
	@[.vms]clean_delete [.src.tds]tds_willconvert.h;*
	@[.vms]clean_delete [.src.tds]num_limits.h;*
	@[.vms]clean_delete [.src.tds]tds_types.h;*
	@[.vms]clean_delete [.src.replacements]iconv_charsets.h;*
	@[.vms]clean_delete [.src.odbc]odbc_export.h;*
	@[.vms]clean_delete [.src.odbc]error_export.h;*

distclean : clean
	#! Files created by configure.com (including descrip.mms)
	@[.vms]clean_delete [.src.odbc]config.h;*
	@[.vms]clean_delete []config.mms;*
	@[.vms]clean_delete []user.mms;*
	@[.vms]clean_delete []descrip.mms;*
	@[.vms]clean_delete []openssl.opt;*

clean :
	#! Files created by descrip.mms
	@[.vms]clean_delete [.include.freetds]sysconfdir.h;*
	@[.vms]clean_delete $(STDINT_H);*
	@[.vms]clean_delete [.include]tds_sysdep_public.h_in;*
	@[.vms]clean_delete [.include]tds_sysdep_public.h;*
	@[.vms]clean_delete [.include]tds_sysdep_types.h_in;*
	@[.vms]clean_delete [.include]tds_sysdep_types.h;*
	@[.vms]clean_delete [.include.readline]readline.h;*
	@[.vms]clean_delete [.include.readline]history.h;*
	@[.vms]clean_delete [.src.utils]util_net.c;*
	@[.vms]clean_delete [.src.replacements]libiconv.c;*
	@[.vms]clean_delete []stage.com;*
	#! Build output and artifacts; test artifacts
	@[.vms]clean_delete [...]*$(OBJ);*
	@[.vms]clean_delete [...]*.LIS;*
	@[.vms]clean_delete [...]*$(OLB);*
	if f$search("[...]*$(E)") .nes. "" then delete/noconfirm [...]*$(E);*/exclude=[.misc...]$(E)
	@[.vms]clean_delete [...unittests]*.ini;*
	@[.vms]clean_delete [...unittests]*.out;*
	@[.vms]clean_delete [...unittests]*.log;*
	if f$search("[...]*.MAP") .nes. "" then delete/noconfirm [...]*.MAP;*/exclude=[.doc...]*.MAP

TDSOBJS = [.src.tds]bulk$(OBJ), [.src.tds]challenge$(OBJ), [.src.tds]config$(OBJ), \
	[.src.tds]convert$(OBJ), [.src.tds]data$(OBJ), [.src.tds]getmac$(OBJ), \
	[.src.tds]gssapi$(OBJ), [.src.tds]iconv$(OBJ), [.src.tds]locale$(OBJ), \
	[.src.tds]login$(OBJ), [.src.tds]mem$(OBJ), [.src.tds]numeric$(OBJ), \
	[.src.tds]query$(OBJ), [.src.tds]read$(OBJ), [.src.utils]tdsstring$(OBJ), \
	[.src.tds]token$(OBJ), [.src.tds]util$(OBJ), \
	[.src.tds]vstrbuild$(OBJ), [.src.tds]write$(OBJ), \
	[.src.tds]net$(OBJ), [.src.tds]tls$(OBJ), [.src.tds]log$(OBJ), [.src.tds]packet$(OBJ), \
	[.src.tds]stream$(OBJ), [.src.tds]random$(OBJ), [.src.tds]sec_negotiate$(OBJ), \
	[.src.replacements]strlcpy$(OBJ), \
	[.src.replacements]strlcat$(OBJ), \
	[.src.utils]des$(OBJ), [.src.utils]md4$(OBJ), [.src.utils]md5$(OBJ), \
	[.src.utils]sleep$(OBJ), \
	[.src.utils]hmac_md5$(OBJ), \
	[.src.utils]getpassarg$(OBJ), \
	[.src.utils]threadsafe$(OBJ), \
	[.src.utils]net$(OBJ), \
	[.src.utils]bjoern-utf8$(OBJ), \
	[.src.utils]smp$(OBJ), \
	[.src.utils]path$(OBJ), \
	[.src.utils]strndup$(OBJ), \
	[.src.utils]tds_cond$(OBJ), \
	[.src.utils]util_net$(OBJ), \
	[.src.utils]ascii$(OBJ), \
	$(ASPRINTFOBJ) $(VASPRINTFOBJ) $(SNPRINTFOBJ) $(STRTOK_ROBJ) $(LIBICONVOBJ) $(SOCKETPAIROBJ) \
	[.vms]getpass$(OBJ)

CTLIBOBJS = [.src.ctlib]blk$(OBJ), [.src.ctlib]cs$(OBJ), [.src.ctlib]ct$(OBJ), \
	[.src.ctlib]ctutil$(OBJ)

DBLIBOBJS = [.src.dblib]bcp$(OBJ), [.src.dblib]dblib$(OBJ), [.src.dblib]dbpivot$(OBJ), $(DBOPENOBJ) \
	[.src.dblib]dbutil$(OBJ), [.src.dblib]rpc$(OBJ), [.src.dblib]xact$(OBJ)

TDSSRVOBJS = [.src.server]query$(OBJ), [.src.server]server$(OBJ), [.src.server]login$(OBJ)

TDSPOOLOBJS = [.src.pool]config$(OBJ), [.src.pool]main$(OBJ), [.src.pool]member$(OBJ), \
	[.src.pool]user$(OBJ), [.src.pool]util$(OBJ)

TDSODBCOBJS = \
    [.src.odbc]bcp$(OBJ), \
    [.src.odbc]connectparams$(OBJ), \
    [.src.odbc]convert_tds2sql$(OBJ), \
    [.src.odbc]descriptor$(OBJ), \
    [.src.odbc]error$(OBJ), \
    [.src.odbc]native$(OBJ), \
    [.src.odbc]odbc$(OBJ), \
    [.src.odbc]odbc_checks$(OBJ), \
    [.src.odbc]odbc_data$(OBJ), \
    [.src.odbc]odbc_util$(OBJ), \
    [.src.odbc]prepare_query$(OBJ), \
    [.src.odbc]sql2tds$(OBJ), \
    [.src.odbc]sqlwchar$(OBJ), \
    [.src.odbc]unixodbc$(OBJ)

# This is the top-level target

_all : libs apps buildchecks stage.com
	@ write sys$output " "
	@ QUALIFIERS := $(MMSQUALIFIERS)
	@ QUALIFIERS = QUALIFIERS - """" - """"
	@ write sys$output " "
	@ write sys$output " Everything is up to date. '$(MMS)''QUALIFIERS' check' to run test suite."

# Configuration dependencies

CONFIGS = [.include]config.h [.include.freetds]sysconfdir.h [.include]tds_sysdep_public.h \
	[.include.readline]readline.h [.include.readline]history.h [.include.freetds]sysdep_types.h

$(TDSOBJS) : $(CONFIGS)

$(CTLIBOBJS) : $(CONFIGS)

$(DBLIBOBJS) : $(CONFIGS)

$(TDSSRVOBJS) : $(CONFIGS)

$(TDSPOOLOBJS) : $(CONFIGS)

$(TDSODBCOBJS) : $(CONFIGS)

[.include]config.h : [.vms]config_h.vms 
	@ write sys$output "Run @[.vms]configure to generate config.h"
	@ exit

[.include.freetds]sysconfdir.h :
	@ open/write sysconfh [.include.freetds]sysconfdir.h
	@ write sysconfh "#define FREETDS_SYSCONFDIR ""/FREETDS_ROOT"""
	@ close sysconfh

$(STDINT_H) :
	@ IF F$SEARCH("$(MMS$TARGET)") .EQS. "" THEN COPY [.vms]stdint.h $(MMS$TARGET)

[.include]tds_sysdep_public.h_in :
	@ IF F$SEARCH("$(MMS$TARGET)") .EQS. "" THEN COPY [.include]tds_sysdep_public^.h.in $(MMS$TARGET)

[.include]tds_sysdep_public.h : [.include]tds_sysdep_public.h_in $(STDINT_H)
	@ open/write vmsconfigtmp vmsconfigtmp.com
	@ write vmsconfigtmp "$ define/user_mode/nolog SYS$OUTPUT _NLA0:"
	@ write vmsconfigtmp "$ edit/tpu/nodisplay/noinitialization -"
	@ write vmsconfigtmp "/section=sys$library:eve$section.tpu$section -"
	@ write vmsconfigtmp "/command=sys$input/output=$(MMS$TARGET) $(MMS$SOURCE)"
	@ write vmsconfigtmp "input_file := GET_INFO (COMMAND_LINE, ""file_name"");"
	@ write vmsconfigtmp "main_buffer:= CREATE_BUFFER (""main"", input_file);"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_int16_type@"",""short"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_int32_type@"",""int"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_int64_type@"",""__int64"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_real32_type@"",""float"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_real64_type@"",""double"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_intptr_type@"",""int"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@dblib_define@"",""#$(DBLIB_DEFINE)"");"
	@ write vmsconfigtmp "out_file := GET_INFO (COMMAND_LINE, ""output_file"");"
	@ write vmsconfigtmp "WRITE_FILE (main_buffer, out_file);"
	@ write vmsconfigtmp "quit;"
	@ write vmsconfigtmp "$ exit"
	@ close vmsconfigtmp
	@ @vmsconfigtmp.com
	@ delete/noconfirm/nolog vmsconfigtmp.com;

[.include.freetds]sysdep_types.h_in :
	@ IF F$SEARCH("$(MMS$TARGET)") .EQS. "" THEN COPY [.include.freetds]sysdep_types^.h.in $(MMS$TARGET)

[.include.freetds]sysdep_types.h : [.include.freetds]sysdep_types.h_in $(STDINT_H)
	@ open/write vmsconfigtmp vmsconfigtmp.com
	@ write vmsconfigtmp "$ define/user_mode/nolog SYS$OUTPUT _NLA0:"
	@ write vmsconfigtmp "$ edit/tpu/nodisplay/noinitialization -"
	@ write vmsconfigtmp "/section=sys$library:eve$section.tpu$section -"
	@ write vmsconfigtmp "/command=sys$input/output=$(MMS$TARGET) $(MMS$SOURCE)"
	@ write vmsconfigtmp "input_file := GET_INFO (COMMAND_LINE, ""file_name"");"
	@ write vmsconfigtmp "main_buffer:= CREATE_BUFFER (""main"", input_file);"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_int16_type@"",""short"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_int32_type@"",""int"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_int64_type@"",""__int64"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_real32_type@"",""float"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_real64_type@"",""double"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@tds_sysdep_intptr_type@"",""int"");"
	@ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
	@ write vmsconfigtmp "eve_global_replace(""@dblib_define@"",""#$(DBLIB_DEFINE)"");"
	@ write vmsconfigtmp "out_file := GET_INFO (COMMAND_LINE, ""output_file"");"
	@ write vmsconfigtmp "WRITE_FILE (main_buffer, out_file);"
	@ write vmsconfigtmp "quit;"
	@ write vmsconfigtmp "$ exit"
	@ close vmsconfigtmp
	@ @vmsconfigtmp.com
	@ delete/noconfirm/nolog vmsconfigtmp.com;

[.include.readline]readline.h :
	@ open/write readlineh $(MMS$TARGET)
	@ write readlineh "char *readline(char *prompt);"
	@ write readlineh "int rl_inhibit_completion;"
	@ write readlineh "/* The following are needed for fisql. */"
	@ write readlineh "FILE **rl_outstream_get_addr(void);"
	@ write readlineh "#define rl_outstream (*rl_outstream_get_addr())"
	@ write readlineh "FILE **rl_instream_get_addr(void);"
	@ write readlineh "#define rl_instream (*rl_instream_get_addr())"
	@ write readlineh "static const char *rl_readline_name = NULL;"
	@ write readlineh "#define rl_bind_key(c,f)      do {} while(0)"
	@ write readlineh "#define rl_reset_line_state() do {} while(0)"
	@ write readlineh "#define rl_on_new_line()      do {} while(0)"
	@ close readlineh 

[.include.readline]history.h :
	@ open/write historyh $(MMS$TARGET)
	@ write historyh "void add_history(const char *s);"
	@ close historyh

# Work around MMS bug that confuses these with files in different
# directories having the same names.

[.src.tds]util$(OBJ) : [.src.tds]util.c

[.src.tds]log$(OBJ) : [.src.tds]log.c

[.src.tds]config$(OBJ) : [.src.tds]config.c

[.src.tds]convert$(OBJ) : [.src.tds]convert.c

[.src.tds]data$(OBJ) : [.src.tds]data.c

[.src.dblib]bcp$(OBJ) : [.src.dblib]bcp.c

[.src.dblib]rpc$(OBJ) : [.src.dblib]rpc.c

[.src.odbc]error$(OBJ) : [.src.odbc]error.c

[.src.tds]login$(OBJ) : [.src.tds]login.c

[.src.server]login$(OBJ) : [.src.server]login.c

[.src.tds]query$(OBJ) : [.src.tds]query.c

[.src.server]query$(OBJ) : [.src.server]query.c

# Hack to avoid having two modules named net in the same library
[.src.utils]util_net.c : [.src.utils]net.c
	COPY $(MMS$SOURCE) $(MMS$TARGET)

[.src.replacements]libiconv.c : [.src.replacements]iconv.c
	copy [.src.replacements]iconv.c [.src.replacements]libiconv.c

# Build the libraries

[]libtds$(OLB) : libtds$(OLB)( $(TDSOBJS) )
	LIBRARY /COMPRESS $(MMS$TARGET) /OUTPUT=$(MMS$TARGET)

[]libct$(OLB) : libct$(OLB)( $(CTLIBOBJS) )
	LIBRARY /COMPRESS $(MMS$TARGET) /OUTPUT=$(MMS$TARGET)

[]libsybdb$(OLB) : libsybdb$(OLB)( $(DBLIBOBJS) )
	LIBRARY /COMPRESS $(MMS$TARGET) /OUTPUT=$(MMS$TARGET)

[]libtdssrv$(OLB) : libtdssrv$(OLB)( $(TDSSRVOBJS) )
	LIBRARY /COMPRESS $(MMS$TARGET) /OUTPUT=$(MMS$TARGET)

[]libtdsodbc$(OLB) : libtdsodbc$(OLB)( $(TDSODBCOBJS), $(TDSOBJS) )
	LIBRARY /COMPRESS $(MMS$TARGET) /OUTPUT=$(MMS$TARGET)

$(TDSODBCSHR) : []libtdsodbc$(OLB)
	link$(LINKFLAGS) $(MMS$SOURCE)/include="odbc"/library -
	,[.vms]odbc_driver_axp.opt/options -
	$(OPENSSL_OPTIONS) -
	/share=$(MMS$TARGET)

LIBS = []libtds$(OLB) []libct$(OLB) []libsybdb$(OLB) []libtdssrv$(OLB) []libtdsodbc$(OLB) $(TDSODBCSHR)

libs : $(LIBS)
	@ continue

#
# Build the utility programs and the pool server
#
apps : freebcp$(E) tsql$(E) bsqldb$(E) defncopy$(E) tdspool$(E) fisql$(E)
	@ continue

# Generate a script to stage the apps
# (VMS does not allow pushing symbols to parent; caller must run script after)
stage.com :
	@ open/write staging stage.com
	@ write staging "$ freebcp  :== $''f$environment("DEFAULT")'freebcp.exe"
	@ write staging "$ tsql     :== $''f$environment("DEFAULT")'tsql.exe"
	@ write staging "$ bsqldb   :== $''f$environment("DEFAULT")'bsqldb.exe"
	@ write staging "$ defncopy :== $''f$environment("DEFAULT")'defncopy.exe"
	@ write staging "$ tdspool  :== $''f$environment("DEFAULT")'tdspool.exe"
	@ write staging "$ fisql    :== $''f$environment("DEFAULT")'fisql.exe"
	@ close staging
	@ write sys$output "Now run @stage.com to define foreign commands."


FREEBCP_OBJS = [.src.apps]freebcp$(OBJ), [.vms]vmsarg_mapping_bcp$(OBJ), [.vms]vmsarg_command_bcp$(OBJ), [.vms]vmsarg_parse$(OBJ)
$(FREEBCP_OBJS) : $(CONFIGS)

freebcp$(E) : []libsybdb$(OLB) []libtds$(OLB) $(FREEBCP_OBJS)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(FREEBCP_OBJS),[]libsybdb$(OLB)/library, []libtds$(OLB)/library $(OPENSSL_OPTIONS)

[.src.apps]freebcp$(OBJ) : [.src.apps]freebcp.c

[.vms]vmsarg_command_bcp$(OBJ) : [.vms]vmsarg_command_bcp.cld
       SET COMMAND/OBJECT=$(MMS$TARGET) $(MMS$SOURCE)


tsql$(E) : [.src.apps]tsql$(OBJ) []libsybdb$(OLB) []libtds$(OLB)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(MMS$SOURCE),[]libsybdb$(OLB)/library, []libtds$(OLB)/library $(OPENSSL_OPTIONS)

[.src.apps]tsql$(OBJ) : [.src.apps]tsql.c $(CONFIGS)

bsqldb$(E) : [.src.apps]bsqldb$(OBJ) []libsybdb$(OLB) []libtds$(OLB)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(MMS$SOURCE),[]libsybdb$(OLB)/library, []libtds$(OLB)/library $(OPENSSL_OPTIONS)

[.src.apps]bsqldb$(OBJ) : [.src.apps]bsqldb.c $(CONFIGS)


DEFNCOPY_OBJS = [.src.apps]defncopy$(OBJ), [.vms]vmsarg_mapping_defncopy$(OBJ), \
				 [.vms]vmsarg_command_defncopy$(OBJ), [.vms]vmsarg_parse$(OBJ)
$(DEFNCOPY_OBJS) : $(CONFIGS)

defncopy$(E) : []libsybdb$(OLB) []libtds$(OLB) $(DEFNCOPY_OBJS)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(DEFNCOPY_OBJS),[]libsybdb$(OLB)/library, []libtds$(OLB)/library $(OPENSSL_OPTIONS)
 
[.src.apps]defncopy$(OBJ) : [.src.apps]defncopy.c []libsybdb$(OLB) []libtds$(OLB)

[.vms]vmsarg_command_defncopy$(OBJ) : [.vms]vmsarg_command_defncopy.cld
       SET COMMAND/OBJECT=$(MMS$TARGET) $(MMS$SOURCE)


tdspool$(E) : []libtdssrv$(OLB) []libtds$(OLB) $(TDSPOOLOBJS)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(TDSPOOLOBJS), []libtdssrv$(OLB)/library, []libtds$(OLB)/library $(OPENSSL_OPTIONS)


FISQL_OBJS = [.src.apps.fisql]fisql$(OBJ), [.src.apps.fisql]interrupt$(OBJ), \
			 [.src.apps.fisql]handlers$(OBJ), [.vms]edit$(OBJ), \
			 [.vms]vmsarg_mapping_isql$(OBJ), [.vms]vmsarg_command_isql$(OBJ), [.vms]vmsarg_parse$(OBJ)
$(FISQL_OBJS) : $(CONFIGS)

fisql$(E) : []libsybdb$(OLB), []libtds$(OLB), $(FISQL_OBJS)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(FISQL_OBJS),[]libsybdb$(OLB)/library, []libtds$(OLB)/library $(OPENSSL_OPTIONS)
 
[.src.apps.fisql]fisql$(OBJ) : [.src.apps.fisql]fisql.c []libsybdb$(OLB) []libtds$(OLB)

[.src.apps.fisql]interrupt$(OBJ) : [.src.apps.fisql]interrupt.c

[.src.apps.fisql]handlers$(OBJ) : [.src.apps.fisql]handlers.c

[.vms]vmsarg_command_isql$(OBJ) : [.vms]vmsarg_command_isql.cld
       SET COMMAND/OBJECT=$(MMS$TARGET) $(MMS$SOURCE)

# Run the test suite

check : buildchecks libtdscheck ctlibcheck dblibcheck $(tdsodbccheck)
	@ write sys$output ""
	@ write sys$output "Test run complete."

PWD : PWD.in
	copy $(MMS$SOURCE) $(MMS$TARGET).

FREETDSCONF :
	@ if f$trnlnm("FREETDSCONF") .EQS. "" THEN WRITE SYS$OUTPUT "Point logical FREETDSCONF to freetds.conf for testing."
	@ if f$trnlnm("FREETDSCONF") .EQS. "" THEN RETURN 44

libtdscheck : PWD FREETDSCONF
	@ write sys$output "Starting $(MMS$TARGET)..."
	@ @[.vms]run_tests_in.com $(TTDIR)

ctlibcheck : PWD FREETDSCONF
	@ write sys$output "Starting $(MMS$TARGET)..."
	@ @[.vms]run_tests_in.com $(CTDIR)

dblibcheck : PWD FREETDSCONF
	@ write sys$output "Starting $(MMS$TARGET)..."
	@ @[.vms]run_tests_in.com $(DTDIR)

tdsodbccheck : PWD FREETDSCONF $(TDSODBCSHR)
	@ write sys$output "Starting $(MMS$TARGET)..."
	@ write sys$output "Configure odbc.ini & odbcinst.ini as required for the LIBODBC build you are using."
	@ write sys$output "Make sure odbc.ini and odbcinst.ini are world-readable, since the tests run under minimal privileges."
	@ create/directory/nolog [.src.odbc._libs]
	@ copy $(TDSODBCSHR) [.src.odbc._libs]
	@ @[.vms]run_tests_in.com $(OTDIR)

buildchecks : $(CONFIGS) libtdstests ctlibtests dblibtests $(TDSODBCSHR) $(ODBCTESTS)
	@ continue

LIBTDSTEST_TARGETS = $(TTDIR)dynamic1$(E) $(TTDIR)iconv_fread$(E) \
	$(TTDIR)numeric$(E) $(TTDIR)t0001$(E) $(TTDIR)t0002$(E) \
	$(TTDIR)t0003$(E) $(TTDIR)t0004$(E) $(TTDIR)t0005$(E) \
	$(TTDIR)t0006$(E) $(TTDIR)t0007$(E) $(TTDIR)t0008$(E) \
	$(TTDIR)convert$(E) $(TTDIR)dataread$(E) \
	$(TTDIR)utf8_1$(E) $(TTDIR)utf8_2$(E) $(TTDIR)utf8_3$(E) \
	$(TTDIR)toodynamic$(E) $(TTDIR)readconf$(E) \
	$(TTDIR)charconv$(E) $(TTDIR)nulls$(E) $(TTDIR)corrupt$(E) \
	$(TTDIR)declarations$(E) $(TTDIR)portconf$(E) $(TTDIR)freeze$(E) \
	$(TTDIR)strftime$(E) $(TTDIR)log_elision$(E) \
	$(TTDIR)convert_bounds$(E) $(TTDIR)tls$(E)

CTLIBTEST_TARGETS = $(CTDIR)t0001$(E) $(CTDIR)t0002$(E) $(CTDIR)t0003$(E) \
	$(CTDIR)t0004$(E) $(CTDIR)t0005$(E) $(CTDIR)cs_convert$(E) \
	$(CTDIR)t0007$(E) $(CTDIR)t0008$(E) $(CTDIR)t0009$(E) \
	$(CTDIR)connect_fail$(E) $(CTDIR)ct_options$(E) \
	$(CTDIR)lang_ct_param$(E) $(CTDIR)array_bind$(E) \
	$(CTDIR)cs_diag$(E) $(CTDIR)get_send_data$(E) \
	$(CTDIR)rpc_ct_param$(E) $(CTDIR)rpc_ct_setparam$(E) \
	$(CTDIR)ct_diagclient$(E) $(CTDIR)ct_diagserver$(E) \
	$(CTDIR)ct_diagall$(E) $(CTDIR)cs_config$(E) \
	$(CTDIR)cancel$(E) $(CTDIR)blk_in$(E) \
	$(CTDIR)blk_out$(E) $(CTDIR)ct_cursor$(E) \
	$(CTDIR)ct_cursors$(E) $(CTDIR)ct_dynamic$(E) \
	$(CTDIR)blk_in2$(E) $(CTDIR)datafmt$(E) $(CTDIR)data$(E) \
	$(CTDIR)rpc_fail$(E) $(CTDIR)row_count$(E) \
	$(CTDIR)all_types$(E) $(CTDIR)long_binary$(E) \
	$(CTDIR)will_convert$(E) $(CTDIR)variant$(E) $(CTDIR)errors$(E) \
	$(CTDIR)ct_command$(E) $(CTDIR)timeout$(E) $(CTDIR)has_for_update$(E)

DBLIBTEST_TARGETS = $(DTDIR)rpc$(E)  $(DTDIR)t0001$(E) $(DTDIR)t0002$(E) \
	$(DTDIR)t0003$(E) $(DTDIR)t0004$(E) $(DTDIR)t0005$(E) \
	$(DTDIR)t0006$(E) $(DTDIR)t0007$(E) $(DTDIR)t0008$(E) \
	$(DTDIR)t0009$(E) $(DTDIR)t0011$(E) \
	$(DTDIR)t0012$(E) $(DTDIR)t0013$(E) $(DTDIR)t0014$(E) \
	$(DTDIR)t0015$(E) $(DTDIR)t0016$(E) $(DTDIR)t0017$(E) \
	$(DTDIR)t0018$(E) $(DTDIR)t0019$(E) $(DTDIR)t0020$(E) \
	$(DTDIR)dbsafestr$(E) $(DTDIR)t0022$(E) $(DTDIR)t0023$(E) \
	$(DTDIR)dbmorecmds$(E) $(DTDIR)bcp$(E) \
	$(DTDIR)thread$(E) $(DTDIR)text_buffer$(E) \
	$(DTDIR)done_handling$(E) $(DTDIR)timeout$(E) \
	$(DTDIR)hang$(E) $(DTDIR)null$(E) \
	$(DTDIR)null2$(E) $(DTDIR)setnull$(E) \
	$(DTDIR)numeric$(E) $(DTDIR)pending$(E) \
	$(DTDIR)cancel$(E) $(DTDIR)spid$(E) $(DTDIR)canquery$(E) \
	$(DTDIR)batch_stmt_ins_sel$(E) $(DTDIR)batch_stmt_ins_upd$(E) \
	$(DTDIR)bcp_getl$(E) $(DTDIR)empty_rowsets$(E) \
	$(DTDIR)string_bind$(E) $(DTDIR)colinfo$(E) \
	$(DTDIR)bcp2$(E) $(DTDIR)proc_limit$(E) \
	$(DTDIR)strbuild$(E)

ODBCTEST_TARGETS = $(OTDIR)t0001$(E) $(OTDIR)t0002$(E) $(OTDIR)t0003$(E) \
	$(OTDIR)moreresults$(E) $(OTDIR)connect$(E) $(OTDIR)print$(E) \
	$(OTDIR)date$(E) $(OTDIR)norowset$(E) $(OTDIR)funccall$(E) \
	$(OTDIR)lang_error$(E) $(OTDIR)tables$(E) \
	$(OTDIR)binary_test$(E) $(OTDIR)moreandcount$(E) \
	$(OTDIR)earlybind$(E) $(OTDIR)putdata$(E) $(OTDIR)params$(E) \
	$(OTDIR)raiserror$(E) $(OTDIR)getdata$(E) $(OTDIR)type$(E) \
	$(OTDIR)transaction$(E) $(OTDIR)genparams$(E) $(OTDIR)preperror$(E) \
	$(OTDIR)prepare_results$(E) $(OTDIR)testodbc$(E) \
	$(OTDIR)data$(E) $(OTDIR)error$(E) $(OTDIR)rebindpar$(E) \
	$(OTDIR)rpc$(E) $(OTDIR)convert_error$(E) $(OTDIR)typeinfo$(E) \
	$(OTDIR)const_params$(E) $(OTDIR)insert_speed$(E) \
	$(OTDIR)compute$(E) $(OTDIR)timeout$(E) \
	$(OTDIR)array$(E) $(OTDIR)array_out$(E) \
	$(OTDIR)cursor1$(E) $(OTDIR)scroll$(E) $(OTDIR)cursor2$(E) \
	$(OTDIR)describecol$(E) $(OTDIR)copydesc$(E) \
	$(OTDIR)prepclose$(E) $(OTDIR)warning$(E) \
	$(OTDIR)paramcore$(E) $(OTDIR)timeout2$(E) $(OTDIR)timeout3$(E) \
	$(OTDIR)connect2$(E) $(OTDIR)timeout4$(E) $(OTDIR)freeclose$(E) \
	$(OTDIR)cursor3$(E) $(OTDIR)cursor4$(E) $(OTDIR)cursor5$(E) \
	$(OTDIR)attributes$(E) $(OTDIR)hidden$(E) $(OTDIR)blob1$(E) \
	$(OTDIR)cancel$(E) $(OTDIR)wchar$(E) $(OTDIR)rowset$(E) \
	$(OTDIR)transaction2$(E) $(OTDIR)reexec$(E) \
	$(OTDIR)mars1$(E)

# note: libtds test "tls" requires libtdsodbc.olb (even if not an ODBC build)
libtdstests : []libtds$(OLB) []libtdsodbc$(OLB) $(LIBTDSTEST_TARGETS)
	@ continue

ctlibtests : []libtds$(OLB) []libct$(OLB) $(CTLIBTEST_TARGETS)
	@ continue

dblibtests : []libtds$(OLB) []libsybdb$(OLB) $(DBLIBTEST_TARGETS)
	@ continue

odbctests : []libtds$(OLB) []libtdsodbc$(OLB) $(ODBCTEST_TARGETS)
	@ continue

#
# libtds test rules
#
# Easier to link common objects into all tests, than to
# specify exactly which tests uses which of the common objects.
#
LIBTDSTEST_COMMON_OBJS = $(UTDIR)test_base$(OBJ) $(TTDIR)common$(OBJ) $(TTDIR)utf8$(OBJ) $(TTDIR)allcolumns$(OBJ)
LIBTDSTEST_OBJS = $(LIBTDSTEST_TARGETS:$(E)=$(OBJ)) $(LIBTDSTEST_COMMON_OBJS) 
$(LIBTDSTEST_OBJS) : $(TTDIR)common.h $(CONFIGS)
$(LIBTDSTEST_TARGETS) : $(LIBTDSTEST_COMMON_OBJS)

{$(TTDIR)}$(OBJ){$(TTDIR)}$(E)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(MMS$SOURCE_LIST),[]libtds$(OLB)/library $(OPENSSL_OPTIONS)

# Extra libraries and include path used by tls test
LOCALINCLUDE = "./src/tds/unittests",
$(TTDIR)tls$(OBJ) : $(TTDIR)tls.c
	$(CC_COMMAND)
LOCALINCLUDE =

$(TTDIR)tls$(E) : $(TTDIR)tls$(OBJ)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(MMS$SOURCE_LIST),[.vms]libodbc.opt/options,[]libtdsodbc$(OLB)/lib $(OPENSSL_OPTIONS)

# ctlib test extra dependencies
$(CTDIR)all_types$(E) : $(CTDIR)all_types$(OBJ) $(TTDIR)allcolumns$(OBJ)

#
# ctlib test rules
#
CTLIBTEST_COMMON_OBJS = $(UTDIR)test_base$(OBJ) $(CTDIR)common$(OBJ)
CTLIBTEST_OBJS = $(CTLIBTEST_TARGETS:$(E)=$(OBJ)) $(CTLIBTEST_COMMON_OBJS) 
$(CTLIBTEST_OBJS) : $(CTDIR)common.h $(CONFIGS)
$(CTLIBTEST_TARGETS) : $(CTLIBTEST_COMMON_OBJS)

{$(CTDIR)}$(OBJ){$(CTDIR)}$(E)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(MMS$SOURCE_LIST),[]libct$(OLB)/library,[]libtds$(OLB)/library $(OPENSSL_OPTIONS)

#
# dblib test rules
#
DBLIBTEST_COMMON_OBJS = $(UTDIR)test_base$(OBJ) $(DTDIR)common$(OBJ)
DBLIBTEST_OBJS = $(DBLIBTEST_TARGETS:$(E)=$(OBJ)) $(DBLIBTEST_COMMON_OBJS)
$(DBLIBTEST_OBJS) : $(DTDIR)common.h $(CONFIGS)
$(DBLIBTEST_TARGETS) : $(DBLIBTEST_COMMON_OBJS)

{$(DTDIR)}$(OBJ){$(DTDIR)}$(E)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(MMS$SOURCE_LIST),[]libsybdb$(OLB)/library,[]libtds$(OLB)/library $(OPENSSL_OPTIONS)

#
# tdsodbc test rules
#
ODBCTEST_COMMON_OBJS = $(OTDIR)common$(OBJ), $(OTDIR)c2string$(OBJ), \
   $(OTDIR)parser$(OBJ), $(UTDIR)test_base$(OBJ)
$(ODBCTEST_TARGETS) : $(ODBCTEST_COMMON_OBJS)
ODBCTEST_OBJS = $(ODBCTEST_TARGETS:$(E)=$(OBJ)) $(ODBCTEST_COMMON_OBJS) $(OTDIR)fake_thread$(OBJ)
$(ODBCTEST_OBJS) : $(OTDIR)common.h $(CONFIGS)

# The link rule includes libtdsodbc.OLB, because TIMEOUT3 and CANCEL test
# a tds_sleep function that isn't exported by the driver.
# Unfortunately we cannot just add the .OLB to the dependencies because
# MMS$SOURCE_LIST doesn't add the /LIBRARY switch to the link command.

{$(OTDIR)}$(OBJ){$(OTDIR)}$(E)
	link$(LINKFLAGS)/exe=$(MMS$TARGET) $(MMS$SOURCE_LIST),[.vms]libodbc.opt/options,[]libtdsodbc$(OLB)/lib $(OPENSSL_OPTIONS)

# tdsodbc test extra dependencies
$(OTDIR)timeout3$(E) : $(OTDIR)timeout3$(OBJ) $(OTDIR)fake_thread$(OBJ)

