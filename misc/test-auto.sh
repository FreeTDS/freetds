#!/bin/sh

# automatic test and report

# set -e

# go to main distro dir
DIR=`dirname $0`
cd "$DIR/.."

# save directory
ORIGDIR="$PWD"
DIR="$ORIGDIR/misc"

output_html () {
	SRC=$1
	DST=$2
	cat "$SRC" | perl -e '$fn = $ARGV[0]; shift @ARGV;
@a = <>;
$_ = join("", @a);
s,&,&amp;,g; s,<,&lt;,g; s,>,&gt;,g;
s,\n\+2:([^\n]*),<span class="error">\1</span>,sg;
s,^2:(.*)$,<span class="error">\1</span>,mg;
s,\n\+3:([^\n]*),<span class="info">\1</span>,sg;
s,^3:(.*)$,<span class="info">\1</span>,mg;
s,^1:,,mg;
$html = "<pre>$_</pre>";
open(TMP, "<$fn") or die("open");
@a = <TMP>;
$a = join("", @a);
close(TMP);
$a =~ s,{CONTENT},$html,;
print $a' "$DIR/tmp1.tmpl" > "$DST"
}

# function to save output
output_save () {
	OUT=$1
	shift
	~/bin/classifier "$@" > "$DIR/$OUT.txt"
	RES=$?
	WARN=0
	if test `cat "$DIR/$OUT.txt" | grep '^+\?2:' | wc -l` != 0; then
		WARN=1
	fi
	ERR=0
	if test $RES != 0; then
		ERR=1
		WARN=0
	fi
	echo RES $RES ERR $ERR WARN $WARN
}

# delete all tests output
find . -name \*.test_output -type f -exec rm {} \;
find "$DIR" -name \*.html -type f -exec rm {} \;

# execute configure
#output_save conf ./configure --enable-extra-checks --with-odbc-nodm=/usr
#if test $RES != 0; then
#	echo "errore in configure"
#	exit 1
#fi

echo Making ...
make clean > /dev/null 2> /dev/null
RES=0
output_save make make
if test $RES != 0; then
	echo "error during make"
	exit 1
fi

echo Testing ...
TESTS_ENVIRONMENT="$DIR/full-test.sh"
export TESTS_ENVIRONMENT ORIGDIR
output_save check make check
if  test $RES != 0; then
	echo "error during make check"
	exit 1;
fi

# parse all tests
echo Processing output ...

output_html "$DIR/make.txt" "$DIR/make.html"
output_html "$DIR/check.txt" "$DIR/check.html"

NUM=1
for CUR in `cat "$DIR/check.txt" | grep 'FULL-TEST:.*:FULL-TEST' | sed 's,.*FULL-TEST:,,g; s,:FULL-TEST.*,,g' `; do
	echo $CUR
	TESTLINE="$CUR"
	
	# split file name and results
	RES2=`echo $CUR | sed 's,^.*:,,'`
	CUR=`echo $CUR | sed "s,:$RES2\$,,"`
	RES1=`echo $CUR | sed 's,^.*:,,'`
	CUR=`echo $CUR | sed "s,:$RES1\$,,"`

	TEST=`echo $CUR | sed "s,^$ORIGDIR/,,g"`
	echo $CUR
	echo $TEST
	echo $NUM

	# make output
	OUT=""
	if test -f "$CUR.test_output"; then
		output_html "$CUR.test_output" "$DIR/test$NUM.html"
		OUT="<a href=\"test$NUM.html\">$TEST</a> "
	fi
	if test -f "$CUR.vg.test_output"; then
		output_html "$CUR.vg.test_output" "$DIR/vgtest$NUM.html"
		OUT="$OUT<a href=\"vgtest$NUM.html\">$TEST (ValGrind)</a>"
	fi

	# replace code in check.txt
	cat "$DIR/check.html" | sed "s,FULL-TEST:$TESTLINE:FULL-TEST,$OUT,g" > "$DIR/check.tmp" && mv "$DIR/check.tmp" "$DIR/check.html"
	
#	rm -f "$CUR.test_output" "$CUR.vg.test_output"
	NUM=`expr $NUM + 1`
done

