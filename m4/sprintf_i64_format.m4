dnl $Id: sprintf_i64_format.m4,v 1.1 2006-03-24 22:00:17 jklowden Exp $
##
# Test for 64bit integer sprintf format specifier
##
AC_DEFUN([SPRINTF_I64_FORMAT],
[tds_i64_format=
for arg in ld lld I64d; do
	AC_TRY_RUN([
#include <stdio.h>
#include <string.h>
int main() {
char buf[20];
$tds_sysdep_int64_type ll = ((($tds_sysdep_int64_type) 0x12345) << 32) + 0x6789abcd;
sprintf(buf, "%$arg", ll);
return strcmp(buf, "320255973501901") != 0;
}
],tds_i64_format=$arg)
	if test "x$tds_i64_format" != x; then
		AC_DEFINE_UNQUOTED(TDS_I64_FORMAT, ["$tds_i64_format"], [define to format string used for 64bit integers])
		break;
	fi
done
])
