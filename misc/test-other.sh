#!/bin/sh

# Additional make check test
# check other tools (like Perl and PHP)
# odbc MUST be configured to point to source driver

# stop on errors
set -e

# set correct directory
DIR=`dirname $0`
cd "$DIR/.."

# save directory
ORIGDIR="$PWD"

# init log
LOG="$PWD/test-other.log"
rm -f "$LOG"
echo "log started" >> "$LOG"

## assure make
#make all
#echo "make all ok" >> "$LOG"

tUID=`cat PWD | grep '^UID=' | sed 's,^....,,g'`
tPWD=`cat PWD | grep '^PWD=' | sed 's,^....,,g'`
tSRV=`cat PWD | grep '^SRV=' | sed 's,^....,,g'`

# Perl
if perl --help > /dev/null 2>&1; then
	DBI_DSN="dbi:ODBC:$tSRV"
	DBI_USER="$tUID"
	DBI_PASS="$tPWD"
	export DBI_DSN DBI_USER DBI_PASS
	# TODO better way
	ODBCHOME=/usr
	export ODBCHOME
	for f in DBD-ODBC-*.tar.gz; do
		DIR=`echo "$f" | sed s,.tar.gz$,,g`
		echo Testing $DIR
		if ! test -d "$DIR"; then
			tar zxvf "$DIR.tar.gz"
			# try to apply patch for Sybase
			cd "$DIR"
			echo 'diff -ru DBD-ODBC-1.13/t/07bind.t DBD-ODBC-1.13my/t/07bind.t
--- DBD-ODBC-1.13/t/07bind.t	2005-02-20 10:09:17.039561424 +0100
+++ DBD-ODBC-1.13my/t/07bind.t	2004-12-18 15:19:11.000000000 +0100
@@ -133,7 +133,7 @@
 		  # expect!
 		  $row[2] = "";
 	       }
-	       if ($row[2] ne $_->[2]) {
+	       if ($row[2] ne $_->[2] && ($dbname ne "sql server" || $row[2] ne " ") ) {
 		  print "Column C value failed! bind value = $bind_val, returned values = $row[0]|$row[1]|$row[2]|$row[3]\n";
 		  return undef;
 	       }
' | patch -p1 || true
			cd ..
		fi
		test -d "$DIR"
		cd "$DIR"
		test -r Makefile || perl Makefile.PL
		make clean
		test -r Makefile || perl Makefile.PL
		make
		make test
	done
fi

echo "all tests ok!!!" >> "$LOG"

