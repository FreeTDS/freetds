#!/bin/sh
echo Function not exported by VMS driver
(nm .libs/libtdsodbc.so | grep ' T ' | sed 's,.* T ,,g' | grep '^\(SQL\|ODBC\)'; cat ../../vms/odbc_driver_axp.opt | grep -v '^!' | grep 'SQL' | sed 's,.*SQL\(.*\)=PROCEDURE.*,SQL\1,g') | sort | uniq -u | grep -v ODBCINSTGetProperties

echo Function not exported by windows driver
(nm .libs/libtdsodbc.so | grep ' T ' | sed 's,.* T ,,g' | grep '^\(SQL\|ODBC\)'; cat ../../win32/FreeTDS.def | grep -v '^;' | grep 'SQL' | sed 's,.*SQL,SQL,g' | perl -pe 's,\r,,g') | sort | uniq -u | grep -v ODBCINSTGetProperties
