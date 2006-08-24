dnl $Id: sprintf_i64_format.m4,v 1.8 2006-08-24 09:38:08 freddy77 Exp $
##
# Test for 64bit integer sprintf format specifier
# ld   64 bit machine
# lld  long long format
# I64d Windows format
# Ld   Watcom compiler format
##
AC_DEFUN([SPRINTF_I64_FORMAT],
[tds_i64_format=

AC_COMPILE_IFELSE(AC_LANG_PROGRAM([
#if !defined(__MINGW32__) || !defined(__MSVCRT__)
this should produce an error!
#endif
],[return 0;]),[tds_i64_format="I64d"])

if test "x$ac_cv_sizeof_long" = "x8"; then
	tds_i64_format=ld
fi

if test "x$tds_i64_format" = "x"; then
	for arg in l ll I64 L; do
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <string.h>
int main() {
char buf[20];
$tds_sysdep_int64_type ll = ((($tds_sysdep_int64_type) 0x12345) << 32) + 0x6789abcd;
sprintf(buf, "%${arg}d", ll);
return strcmp(buf, "320255973501901") != 0;
}
]])],[tds_i64_format="${arg}d"; break])
	done
fi
if test "x$tds_i64_format" != "x"; then
	AC_DEFINE_UNQUOTED(TDS_I64_FORMAT, ["$tds_i64_format"], [define to format string used for 64bit integers])
fi
])
