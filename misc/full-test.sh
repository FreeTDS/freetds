#!/bin/sh

# test a test using watching for errors and logging all

#execute with valgrind
RES2=0
VG=0
rm -f "$1.vg.test_output" "$1.test_output"
if test -f "$HOME/bin/vg_test"; then
	~/bin/classifier --num-fd=3 "$HOME/bin/vg_test" "$@" > "$1.vg.test_output"
	RES2=$?
	VG=1
fi

# try to execute normally
RES1=0
if test $RES2 != 0 -o $VG = 0; then
	~/bin/classifier "$@" > "$1.test_output"
	RES1=$?
fi

# log test executed to retrieve lately by main script
FILE=`echo "$1" | sed "s,^./,$PWD/,"`
echo "FULL-TEST:$FILE:$RES1:$RES2:FULL-TEST"

# return always succes, test verified later
exit 0
