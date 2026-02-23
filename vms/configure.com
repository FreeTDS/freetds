$! FreeTDS - Library of routines accessing Sybase and Microsoft databases
$! Copyright (C) 2003  Craig A. Berry   craigberry@mac.com      1-FEB-2003
$! 
$! This library is free software; you can redistribute it and/or
$! modify it under the terms of the GNU Library General Public
$! License as published by the Free Software Foundation; either
$! version 2 of the License, or (at your option) any later version.
$! 
$! This library is distributed in the hope that it will be useful,
$! but WITHOUT ANY WARRANTY; without even the implied warranty of
$! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
$! Library General Public License for more details.
$! 
$! You should have received a copy of the GNU Library General Public
$! License along with this library; if not, write to the
$! Free Software Foundation, Inc., 59 Temple Place - Suite 330,
$! Boston, MA 02111-1307, USA.
$!
$! CONFIGURE.COM -- run from top level source directory as @[.vms]configure
$!
$! Checks for C library functions and applies its findings to 
$! description file template and config.h.  Much of this cribbed
$! from Perl's configure.com, largely the work of Peter Prymmer.
$!
$ SAY := "write sys$output"
$!
$! Extract the version string from version.h
$ SEARCH [.include.freetds]version.h "#define TDS_VERSION_NO"/EXACT/OUTPUT=version.tmp
$ open/read version version.tmp
$ read version versionline
$ close version
$ delete/noconfirm/nolog version.tmp;*
$ quote = """"
$ vers1 = f$element(1, quote, versionline)
$! Expect "FreeTDS v1.6.dev" etc. - want to put the 1.6.dev bit into config.h
$ offset = f$locate("v", vers1)
$ versionstring = f$extract(offset+1, 99, vers1)
$ write sys$output "Version: ''versionstring'"
$ if versionstring .EQS. "" THEN EXIT 44
$ gosub check_crtl
$!
$! The system-supplied iconv() is fine, but unless the internationalization
$! kit has been installed, we may not have the conversions we need.  Check
$! for their presence and use the homegrown iconv() if necessary.
$!
$ IF -
    "FALSE" - ! native iconv() buggy, don't use for now
    .AND. F$SEARCH("SYS$I18N_ICONV:UCS-2_ISO8859-1.ICONV") .NES. "" -
    .AND. F$SEARCH("SYS$I18N_ICONV:ISO8859-1_UCS-2.ICONV") .NES. "" -
    .AND. F$SEARCH("SYS$I18N_ICONV:UTF-8_ISO8859-1.ICONV") .NES. "" -
    .AND. F$SEARCH("SYS$I18N_ICONV:ISO8859-1_UTF-8.ICONV") .NES. ""
$ THEN
$   d_have_iconv = "1"
$   SAY "Using system-supplied iconv()"
$ ELSE
$   d_have_iconv = "0"
$   SAY "Using replacement iconv()"
$ ENDIF
$!
$! Set socketpair (available with VMS 8.2 and later)
$!
$ IF F$EXTRACT(1,3,F$EDIT(F$GETSYI("VERSION"),"TRIM")) .GES. "8.2"
$ THEN
$   d_socketpair = "1"
$   SAY "Using system-supplied socketpair()"
$ ELSE
$   d_socketpair = "0"
$   SAY "Using replacement socketpair()"
$ ENDIF
$!
$!
$! Detect OpenSSL version - Logical OPENSSL must be defined in order
$! for headers to be found (#include <openssl/...> works by logical
$! substitution).
$!
$ sslval = F$ELEMENT(0,"$",F$TRNLNM("OPENSSL"))
$ IF sslval .NES. ""
$ THEN
$!  The version number; could be blank string if they are just using
$!  SSL$ROOT without version number
$   sslver = F$EXTRACT(3,F$LENGTH(sslval)-3,sslval)
$   d_openssl = "1"
$   SAY "Found OpenSSL ''sslver' and creating linker options file..."
$   OPEN/WRITE sslopt openssl.opt
$   IF F$TRNLNM("FREETDS_OPENSSL_STATIC") .NE. 0
$   THEN
$     SAY "OpenSSL static linking."
$     WRITE sslopt "SSL''sslver'$LIB:SSL''sslver'$LIBSSL32.OLB/LIB"
$     WRITE sslopt "SSL''sslver'$LIB:SSL''sslver'$LIBCRYPTO32.OLB/LIB"
$   ELSE
$     SAY "OpenSSL linking to shared image."
$     WRITE sslopt "SYS$SHARE:SSL''sslver'$LIBSSL_SHR32.EXE/SHARE"
$     WRITE sslopt "SYS$SHARE:SSL''sslver'$LIBCRYPTO_SHR32.EXE/SHARE"
$   ENDIF
$   CLOSE sslopt
$ ELSE
$   d_openssl = "0"
$   SAY "Did not find OpenSSL"
$ ENDIF
$!
$! Generate config.h
$!
$ open/write vmsconfigtmp vmsconfigtmp.com
$ write vmsconfigtmp "$ define/user_mode/nolog SYS$OUTPUT _NLA0:"
$ write vmsconfigtmp "$ edit/tpu/nodisplay/noinitialization -"
$ write vmsconfigtmp "/section=sys$library:eve$section.tpu$section -"
$ write vmsconfigtmp "/command=sys$input/output=[.include]config.h [.vms]config_h.vms"
$ write vmsconfigtmp "input_file := GET_INFO (COMMAND_LINE, ""file_name"");"
$ write vmsconfigtmp "main_buffer:= CREATE_BUFFER (""main"", input_file);"
$ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
$ write vmsconfigtmp "eve_global_replace(""@D_ASPRINTF@"",""''d_asprintf'"");"
$ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
$ write vmsconfigtmp "eve_global_replace(""@D_VASPRINTF@"",""''d_vasprintf'"");"
$ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
$ write vmsconfigtmp "eve_global_replace(""@D_STRTOK_R@"",""''d_strtok_r'"");"
$ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
$ write vmsconfigtmp "eve_global_replace(""@VERSION@"",""""""''versionstring'"""""");"
$ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
$ write vmsconfigtmp "eve_global_replace(""@D_HAVE_ICONV@"",""''d_have_iconv'"");"
$ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
$ write vmsconfigtmp "eve_global_replace(""@D_SNPRINTF@"",""''d_snprintf'"");"
$ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
$ write vmsconfigtmp "eve_global_replace(""@D_SOCKETPAIR@"",""''d_socketpair'"");"
$ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
$ write vmsconfigtmp "eve_global_replace(""@D_OPENSSL@"",""''d_openssl'"");"
$ write vmsconfigtmp "POSITION (BEGINNING_OF (main_buffer));"
$ write vmsconfigtmp "eve_global_replace(""@D_STDINT@"",""''d_stdint'"");"
$ write vmsconfigtmp "out_file := GET_INFO (COMMAND_LINE, ""output_file"");"
$ write vmsconfigtmp "WRITE_FILE (main_buffer, out_file);"
$ write vmsconfigtmp "quit;"
$ write vmsconfigtmp "$ exit"
$ close vmsconfigtmp
$ @vmsconfigtmp.com
$ delete/noconfirm/nolog vmsconfigtmp.com;
$!
$! Options for descrip.mms
$!
$ open/write vmsconfigtmp config.mms
$ write vmsconfigtmp "D_OPENSSL = ''d_openssl'"
$ write vmsconfigtmp "D_STDINT = ''d_stdint'"
$ write vmsconfigtmp "D_ASPRINTF = ''d_asprintf'"
$ write vmsconfigtmp "D_VASPRINTF = ''d_vasprintf'"
$ write vmsconfigtmp "D_STRTOK_R = ''d_strtok_r'"
$ write vmsconfigtmp "D_ICONV = ''d_have_iconv'"
$ write vmsconfigtmp "D_SNPRINTF = ''d_snprintf'"
$ write vmsconfigtmp "D_SOCKETPAIR = ''d_socketpair'"
$ if P1 .nes. "--disable-thread-safe" then write vmsconfigtmp  "ENABLE_THREAD_SAFE = 1"
$ close vmsconfigtmp
$ open/write vmsconfigtmp descrip.mms
$ write vmsconfigtmp "include config.mms"
$ write vmsconfigtmp "include [.vms]descrip.mms"
$ close vmsconfigtmp
$!
$ Say ""
$ Say "Configuration complete; run MMK to build."
$ Say "Sample build command: mmk/MACRO=(""MSDBLIB""=1,""ODBC""=1,""ODBC_MARS""=1,""ODBC_WIDE""=1)"
$ Say "  append 'check' to run tests"
$ EXIT
$!
$ CHECK_CRTL:
$!
$ OS := "open/write CONFIG []try.c"
$ WS := "write CONFIG"
$ CS := "close CONFIG"
$ DS := "delete/nolog/noconfirm []try.*;*"
$ good_compile = %X10B90001
$ good_link = %X10000001
$ tmp = "" ! null string default
$!
$! Check for asprintf
$!
$ OS
$ WS "#include <stdio.h>"
$ WS "#include <stdlib.h>"
$ WS "int main(void)"
$ WS "{"
$ WS "char *ptr;
$ WS "asprintf(&ptr,""%d"",1);"
$ WS "exit(0);"
$ WS "}"
$ CS
$ tmp = "asprintf"
$ GOSUB inlibc
$ d_asprintf == tmp
$!
$!
$! Check for vasprintf
$!
$ OS
$ WS "#include <stdio.h>"
$ WS "#include <stdlib.h>"
$ WS "#include <stdarg.h>"
$ WS "void try_vasprintf(const char *fmt, ...)"
$ WS "{"
$ WS "    char* dyn_buf;"
$ WS "    va_list args;"
$ WS "    va_start(args, fmt);"
$ WS "    const int written = vasprintf(&dyn_buf, fmt, args);"
$ WS "    va_end(args);"
$ WS "    free(dyn_buf);"
$ WS "    if (written == 18) exit(0);"
$ WS "    exit(1);"
$ WS "}"
$ WS "int main(void)"
$ WS "{"
$ WS "    try_vasprintf(""Testing... %d, %d, %d"", 1, 2, 3);"
$ WS "}"
$ CS
$ tmp = "vasprintf"
$ GOSUB inlibc
$ d_vasprintf == tmp
$!
$!
$!
$! Check for strtok_r
$!
$ OS
$ WS "#include <stdlib.h>"
$ WS "#include <string.h>"
$ WS "int main(void)"
$ WS "{"
$ WS "char *word, *brkt, mystr[4];"
$ WS "strcpy(mystr,""1^2"");"
$ WS "word = strtok_r(mystr, ""^"", &brkt);"
$ WS "exit(0);"
$ WS "}"
$ CS
$ tmp = "strtok_r"
$ GOSUB inlibc
$ d_strtok_r == tmp
$!
$!
$! Check for snprintf
$!
$ OS
$ WS "#include <stdarg.h>"
$ WS "#include <stdio.h>"
$ WS "#include <stdlib.h>"
$ WS "int main(void)"
$ WS "{"
$ WS "char ptr[15];"
$ WS "snprintf(ptr,sizeof(ptr),""%d,%d"",1,2);"
$ WS "exit(0);"
$ WS "}"
$ CS
$ tmp = "snprintf"
$ GOSUB inlibc
$ d_snprintf == tmp
$!
$!
$! Check for stdint.h
$!
$ OS
$ WS "#include <stdlib.h>"
$ WS "#include <stdint.h>"
$ WS "int main(void)"
$ WS "{"
$ WS "exit(0);"
$ WS "}"
$ CS
$ tmp = "stdint"
$ GOSUB inlibc
$ d_stdint == tmp
$!
$ DS
$ RETURN
$!********************
$inlibc: 
$ GOSUB link_ok
$ IF compile_status .EQ. good_compile .AND. link_status .EQ. good_link
$ THEN
$   say "''tmp'() found."
$   tmp = "1"
$ ELSE
$   say "''tmp'() NOT found."
$   tmp = "0"
$ ENDIF
$ RETURN
$!
$!: define a shorthand compile call
$compile:
$ GOSUB link_ok
$just_mcr_it:
$ IF compile_status .EQ. good_compile .AND. link_status .EQ. good_link
$ THEN
$   OPEN/WRITE CONFIG []try.out
$   DEFINE/USER_MODE SYS$ERROR CONFIG
$   DEFINE/USER_MODE  SYS$OUTPUT CONFIG
$   MCR []try.exe
$   CLOSE CONFIG
$   OPEN/READ CONFIG []try.out
$   READ CONFIG tmp
$   CLOSE CONFIG
$   DELETE/NOLOG/NOCONFIRM []try.out;
$   DELETE/NOLOG/NOCONFIRM []try.exe;
$ ELSE
$   tmp = "" ! null string default
$ ENDIF
$ RETURN
$!
$link_ok:
$ GOSUB compile_ok
$ DEFINE/USER_MODE SYS$ERROR _NLA0:
$ DEFINE/USER_MODE SYS$OUTPUT _NLA0:
$ SET NOON
$ LINK try.obj
$ link_status = $status
$ SET ON
$ IF F$SEARCH("try.obj") .NES. "" THEN DELETE/NOLOG/NOCONFIRM try.obj;
$ RETURN
$!
$!: define a shorthand compile call for compilations that should be ok.
$compile_ok:
$ DEFINE/USER_MODE SYS$ERROR _NLA0:
$ DEFINE/USER_MODE SYS$OUTPUT _NLA0:
$ SET NOON
$ CC try.c
$ compile_status = $status
$ SET ON
$ DELETE/NOLOG/NOCONFIRM try.c;
$ RETURN
$!
$beyond_compile_ok:
$!
