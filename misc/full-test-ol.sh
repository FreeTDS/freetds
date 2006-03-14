#!/bin/sh

# test a test using watching for errors and logging all

online_log () {
	echo "@!@!@!@!@!@!@!@!@!@!@!@!@!@!@!@- $1 -@!@!@!@!@!@!@!@!@!@!@!@!@!@!@!@"
}

#execute with valgrind
rm -f "$1.test_output"

# execute normally
online_log "START $1"
classifier "$@"
RES1=$?
online_log "RESULT $RES1"

# log test executed to retrieve lately by main script
FILE=`echo "$1" | sed "s,^\\./,$PWD/,"`
online_log "FILE $FILE"
online_log "END $1"

# return always succes, test verified later
exit 0
