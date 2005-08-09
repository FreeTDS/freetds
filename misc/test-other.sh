#!/bin/sh

# Additional make check test
# check other tools (like Perl and PHP)
# odbc MUST be configured to point to source driver

# stop on errors
set -e

verbose=no
do_perl=no
do_php=yes

# check for perl existence
if perl --help > /dev/null 2>&1; then
	do_perl=yes
fi

for param
do
	case $param in
	--verbose)
		verbose=yes ;;
	--no-perl)
		do_perl=no ;;
	--no-php)
		do_php=no ;;
	*)
		echo 'Option not supported!' 1>&2
		exit 1
		;;
	esac
done

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
if test $do_perl = yes; then
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
		if test $verbose = yes; then
			make test TEST_VERBOSE=1
		else
			make test
		fi
	done
fi

# PHP
cd "$ORIGDIR"
FILE=php5-latest.tar.gz
if test $do_php = yes -a -f "$FILE"; then
	# need to recompile ??
	if test ! -x phpinst/bin/php -o "$FILE" -nt phpinst/bin/php; then
		rm -rf php5-200* phpinst lib
		tar zxvf $FILE
		DIR=`echo php5-200*`
		MAINDIR=$PWD
		mkdir lib
		cp src/dblib/.libs/lib*.so* lib
		cp src/tds/.libs/lib*.so* lib
		cd $DIR
		./configure --prefix=$MAINDIR/phpinst --disable-all --with-mssql=$MAINDIR
		make
		make install
		cd ..
	fi

	cd phptests
	echo "<?php \$server = \"$tSRV\"; \$user = \"$tUID\"; \$pass = \"$tPWD\"; ?>" > pwd.inc

	ERR=""
	for f in *.php; do
		echo Testing PHP script $f
		../phpinst/bin/php -q $f || ERR="$ERR $f"
	done
	if test "$ERR" != ""; then
		echo "Some script failed:$ERR"
		exit 1
	fi
fi

echo "all tests ok!!!" >> "$LOG"
exit 0

