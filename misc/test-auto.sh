#!/bin/sh

# automatic test and report

# set -e

# go to main distro dir
DIR=`dirname $0`
cd "$DIR/.."

# save directory
ORIGDIR="$PWD"
DIR="$ORIGDIR/misc"
LANG=C
export LANG

INFO="<table border=\"1\">
  <tr>
    <th>Hostname</th>
    <td>`hostname | sed 's,\..*$,,; s,&,&amp;,g; s,<,&lt;,g; s,>,&gt;,g'`</td>
  </tr>
  <tr>
    <th>gcc version</th>
    <td>`gcc --version 2> /dev/null | grep 'GCC' | sed 's,&,&amp;,g; s,<,&lt;,g; s,>,&gt;,g'`</td>
  </tr>
  <tr>
    <th>uname -a</th>
    <td>`uname -a | sed 's,&,&amp;,g; s,<,&lt;,g; s,>,&gt;,g'`</td>
  </tr>
  <tr>
    <th>date</th>
    <td>`date '+%Y-%m-%d'`</td>
  </tr>
</table>
<br />"

# create html template
echo "<html>
<head>
<title>{TITLE}</title>
<style>
.error { color: red }
.info  { color: blue }
</style>
</head>
<body>
<h1>{TITLE}</h1>
<p><a href=\"out.html\">Main</a></p>
$INFO
{CONTENT}
<p><a href=\"out.html\">Main</a></p>
</body>
</html>
" > "$DIR/tmp1.tmpl"

output_html () {
	SRC=$2
	DST=$3
	cat "$SRC" | perl -e '$fn = shift @ARGV; $title = shift @ARGV;
@a = <>;
$_ = join("", @a);
s,&,&amp;,g; s,<,&lt;,g; s,>,&gt;,g;
$* = 0;
s,\n\+2:([^\n]*),<span class="error">\1</span>,g;
s,\n\+3:([^\n]*),<span class="info">\1</span>,g;
$* = 1;
s,^2:(.*)$,<span class="error">\1</span>,g;
s,^3:(.*)$,<span class="info">\1</span>,g;
s,^1:,,g;
$html = "<pre>$_</pre>";
open(TMP, "<$fn") || die("open");
@a = <TMP>;
$a = join("", @a);
close(TMP);
$_ = $title;
s,&,&amp;,g; s,<,&lt;,g; s,>,&gt;,g;
$a =~ s,{TITLE},$_,g;
$a =~ s,{CONTENT},$html,;
print $a' "$DIR/tmp1.tmpl" "$1"  > "$DST"
}

# function to save output
output_save () {
	COMMENT=$1
	shift
	OUT=$1
	shift
	classifier "$@" > "$DIR/$OUT.txt"
	RES=$?

	cat "$DIR/$OUT.txt" | grep -v '^2:.*\(has modification time in the future \|Clock skew detected\.  Your build may be incomplete\.\|Current time: Timestamp out of range; substituting \)' > "$DIR/$OUT.tmp" && mv -f "$DIR/$OUT.tmp" "$DIR/$OUT.txt"
	output_html "$COMMENT" "$DIR/$OUT.txt" "$DIR/$OUT.html"

	WARN="no :-)"
	if test `cat "$DIR/$OUT.txt" | sed 's,^+2:,2:,g' | grep '^2:' | wc -l` != 0; then
		WARN="yes :-("
	fi
	ERR="yes :-)"
	if test $RES != 0; then
		ERR="no :-("
		WARN=ignored
	fi
	
	# output row information
	out_row "$COMMENT  $ERR  $WARN  <a href=\"$OUT.html\">log</a>"
}

out_init () {
	echo "<html>
<body>
$INFO
" > "$DIR/out.html"
}

out_header () {
	echo "$1" | sed 's,   *,</th><th>,g; s,^,<table border="1"><tr><th>,; s,$,</th></tr>,' >> "$DIR/out.html"
}

out_row () {
	echo "$1" | sed 's,   *,</td><td>,g; s,^,<tr><td>,; s,$,</td></tr>,; s,<td>\([^<]*\):-)</td>,<td><font color="green">\1</font></td>,g; s,<td>\([^<]*\):-(</td>,<td bgcolor="red">\1</td>,g; s,<td>\([^<]*\):(</td>,<td bgcolor="yellow">\1</td>,g' >> "$DIR/out.html"
}

out_footer () {
	echo "</table><br />" >> "$DIR/out.html"
}

out_end () {
	echo "</body></html>" >> "$DIR/out.html"
}

# delete all tests output
find . -name \*.test_output -type f -exec rm {} \;
find "$DIR" -name \*.html -type f -exec rm {} \;

# execute configure
#output_save "configuration" conf ./configure --enable-extra-checks --with-odbc-nodm=/usr
#if test $RES != 0; then
#	echo "errore in configure"
#	exit 1
#fi

out_init
out_header "Operation  Success  Warnings  Log"

echo Making ...
MAKE=make
if gmake --help 2> /dev/null > /dev/null; then
	MAKE=gmake
fi
$MAKE clean > /dev/null 2> /dev/null
output_save "make" make $MAKE
if test $RES != 0; then
	out_footer
	out_end
	echo "error during make"
	exit 1
fi

echo Making tests ...
TESTS_ENVIRONMENT=true
export TESTS_ENVIRONMENT
output_save "make tests" maketest $MAKE check 
if  test $RES != 0; then
	out_footer
	out_end
	echo "error during make test"
	exit 1;
fi
out_footer

echo Testing ...
TESTS_ENVIRONMENT="$DIR/full-test.sh"
export TESTS_ENVIRONMENT ORIGDIR
$MAKE check 2> /dev/null > "$DIR/check.txt"
if  test $RES != 0; then
	out_end
	echo "error during make check"
	exit 1;
fi

# parse all tests
echo Processing output ...

NUM=1
out_header "Tests  Success  Warnings  log  VG Success  VG warnings  VG errors  VG leaks  VG log"
for CUR in `cat "$DIR/check.txt" | grep 'FULL-TEST:.*:FULL-TEST' | sed 's,.*FULL-TEST:,,g; s,:FULL-TEST.*,,g' `; do
	TESTLINE="$CUR"
	
	# split file name and results
	RES2=`echo $CUR | sed 's,^.*:,,'`
	CUR=`echo $CUR | sed "s,:$RES2\$,,"`
	RES1=`echo $CUR | sed 's,^.*:,,'`
	CUR=`echo $CUR | sed "s,:$RES1\$,,"`

	TEST=`echo $CUR | sed "s,^$ORIGDIR/,,g"`
	echo $TEST

	# patch make test page
	OUT=""
	if test -f "$CUR.test_output"; then
		output_html "$TEST" "$CUR.test_output" "$DIR/test$NUM.html"
		OUT="<a href=\"test$NUM.html\">$TEST</a> "
	fi
	if test -f "$CUR.vg.test_output"; then
		output_html "$TEST" "$CUR.vg.test_output" "$DIR/vgtest$NUM.html"
		OUT="$OUT<a href=\"vgtest$NUM.html\">$TEST (ValGrind)</a>"
	fi

	# make output
	LOG1="not present"
	WARN1=unknown
	ERR1=unknown
	if test -f "$CUR.test_output"; then
		ERR1="yes :-)"
		WARN1="no :-)"
		LOG1="<a href=\"test$NUM.html\">log</a>"
		if test `cat "$CUR.test_output" | sed 's,^+2:,2:,g' | grep '^2:' | wc -l` != 0; then
			WARN1="yes :("
		fi
		if test $RES1 != 0; then
			ERR1="no :-("
			WARN1=ignored
		fi
	fi
	
	LOG2="not present"
	WARN2=unknown
	ERR2=unknown
	LEAK=unknown
	VGERR=unknown
	if test -f "$CUR.vg.test_output"; then
		WARN2="no :-)"
		ERR2="yes :-)"
		LEAK="no :-)"
		VGERR="no :-)"
		LOG2="<a href=\"vgtest$NUM.html\">log</a>"
		if test `cat "$CUR.vg.test_output" | sed 's,^+2:,2:,g' | grep '^2:' | wc -l` != 0; then
			WARN2="yes :("
		fi
		if test `cat "$CUR.vg.test_output" | grep ':==.*no leaks are possible' | wc -l` == 0; then
			LEAK="yes :-("
		fi
		if test `cat "$CUR.vg.test_output" | grep 'ERROR SUMMARY: 0 errors from 0 contexts' | wc -l` == 0; then
			VGERR="yes :-("
		fi
		if test $RES2 != 0; then
			ERR2="no :-("
			WARN2=ignored
		fi
	fi

	out_row "$TEST  $ERR1  $WARN1  $LOG1  $ERR2  $WARN2  $VGERR  $LEAK  $LOG2"

	
#	rm -f "$CUR.test_output" "$CUR.vg.test_output"
	NUM=`expr $NUM + 1`
done
out_footer

out_end
