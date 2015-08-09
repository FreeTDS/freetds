#!/bin/sh

EXP="nm .libs/libtdsodbc.so | grep ' T ' | sed 's,.* T ,,g'"

echo Function not exported by VMS driver
(eval "$EXP" | grep '^\(SQL\|ODBC\)'; cat ../../vms/odbc_driver_axp.opt | grep -v '^!' | grep 'SQL' | sed 's,.*SQL\(.*\)=PROCEDURE.*,SQL\1,g') | sort | uniq -u | grep -v ODBCINSTGetProperties

echo Function not exported by odbc driver
(eval "$EXP" | grep '^\(SQL\|ODBC\)'; cat odbc_w.def | grep -v '^;' | grep 'SQL' | sed 's,.*SQL,SQL,g' | perl -pe 's,\r,,g') | sort | uniq -u | grep -v ODBCINSTGetProperties

echo Function not declared as implemented
(eval "$EXP" | grep '^SQL' | grep -v '[a-z]W$' | perl -pe 'tr/a-z/A-Z/'; cat odbc.c | grep 'X(SQL_API_' | sed 's,.*SQL_API_SQL,SQL,g; s,).*,,g' | sort | uniq) | sort | uniq -u
(eval "$EXP" | grep '^SQL' | grep -v '[a-z]W$' | perl -pe 'tr/a-z/A-Z/'; cat odbc.c | grep '_(SQL_API_' | sed 's,.*SQL_API_SQL,SQL,g; s,).*,,g' | sort | uniq) | sort | uniq -d

echo
echo "DBLIB"
cd ../dblib
EXP="nm .libs/libsybdb.so | grep ' T ' | sed 's,.* T ,,g'"
(eval "$EXP"; cat dblib.def | perl -pe 's,\r,,g; s,^\s+,,; s,^\n,,s; s,^EXPORTS\n,,s') | sort | uniq -u


echo
echo "Ctlib"
cd ../ctlib
EXP="nm .libs/libct.so | grep ' T ' | sed 's,.* T ,,g'"
(eval "$EXP"; cat ctlib.def | perl -pe 's,\r,,g; s,^\s+,,; s,^\n,,s; s,^EXPORTS\n,,s') | sort | uniq -u
