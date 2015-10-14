#!/bin/sh

# run a test watching for errors and logging all

log () {
	echo "@!@!@!@!@!@!@!@!@!@!@!@!@!@!@!@- $1 -@!@!@!@!@!@!@!@!@!@!@!@!@!@!@!@"
}

cd "`dirname $0`/.."
DIR="`pwd -P`"
cd - > /dev/null

FILE=`echo "$1" | sed "s,^\\./,$PWD/,"`

# execute with valgrind
RES=1
if test -f "$HOME/bin/vg_test"; then
	log "START $1"
	log "TEST 1"
	log "VALGRIND 1"
	"$DIR/misc/grabcov" -o "$FILE.cov_info" -- classifier --timeout=600 --num-fd=3 "$HOME/bin/vg_test" "$@"
	RES=$?
	log "RESULT $RES"
	log "FILE $FILE"
	log "END $1"
fi

# try to execute normally
if test $RES != 0 -a $RES != 77; then
	log "START $1"
	log "TEST 1"
	"$DIR/misc/grabcov" -o "$FILE.cov_info" -- classifier --timeout=600 "$@"
	RES=$?
	log "RESULT $RES"

	# log test executed to retrieve lately by main script
	log "FILE $FILE"
	log "END $1"
fi

# return always success, test verified later
exit 0

