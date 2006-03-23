#!/bin/sh

# test a test using watching for errors and logging all

online_log () {
	echo "@!@!@!@!@!@!@!@!@!@!@!@!@!@!@!@- $1 -@!@!@!@!@!@!@!@!@!@!@!@!@!@!@!@"
}

FILE=`echo "$1" | sed "s,^\\./,$PWD/,"`

#execute with valgrind
RES=0
VG=0
if test -f "$HOME/bin/vg_test"; then
	online_log "START $1"
	online_log "TEST 1"
	online_log "VALGRIND 1"
	classifier --timeout=600 --num-fd=3 "$HOME/bin/vg_test" "$@"
	RES=$?
	online_log "RESULT $RES"
	online_log "FILE $FILE"
	online_log "END $1"
	VG=1
fi

# try to execute normally
if test $RES != 0 -o $VG = 0; then
	online_log "START $1"
	online_log "TEST 1"
	classifier --timeout=600 "$@"
	RES=$?
	online_log "RESULT $RES"

	# log test executed to retrieve lately by main script
	online_log "FILE $FILE"
	online_log "END $1"
fi

# return always succes, test verified later
exit 0

