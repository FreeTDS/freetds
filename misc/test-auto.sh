#!/bin/sh

# automatic test and report

# set -e

BUILD=1
CONFIGURE=0
for param
do
	case $param in
	--no-build)
		BUILD=0
		;;
	--configure)
		CONFIGURE=1
		;;
	esac
done

# go to main distro dir
DIR=`dirname $0`
cd "$DIR/.."

DIR="$PWD/misc"
LANG=C
export LANG

log () {
	echo "@!@!@!@!@!@!@!@!@!@!@!@!@!@!@!@- $1 -@!@!@!@!@!@!@!@!@!@!@!@!@!@!@!@"
}

# function to save output
output_save () {
	COMMENT=$1
	shift
	OUT=$1
	shift
	log "START $OUT"
	classifier "$@"
	RES=$?
	log "RESULT $RES"
	log "END $OUT"
}

# output informations
log "INFO HOSTNAME $(echo $(hostname))"
VER=$(gcc --version 2> /dev/null | grep 'GCC')
log "INFO GCC $VER"
log "INFO UNAME $(echo $(uname -a))"
log "INFO DATE $(date '+%Y-%m-%d')"

MAKE=make
if gmake --help 2> /dev/null > /dev/null; then
	MAKE=gmake
fi

# execute configure
if test $CONFIGURE = 1; then
	BUILD=1
	output_save "configuration" conf ./configure --enable-extra-checks $TDS_AUTO_CONFIG
	if test $RES != 0; then
		echo "error during configure"
		exit 1
	fi
fi

if test $BUILD = 1; then

	echo Making ...
	$MAKE clean > /dev/null 2> /dev/null
	output_save "make" make $MAKE
	if test $RES != 0; then
		echo "error during make"
		exit 1
	fi

	echo Making tests ...
	TESTS_ENVIRONMENT=true
	export TESTS_ENVIRONMENT
	output_save "make tests" maketest $MAKE check 
	if  test $RES != 0; then
		echo "error during make test"
		exit 1;
	fi
fi

echo Testing ...
TESTS_ENVIRONMENT="$DIR/full-test.sh"
export TESTS_ENVIRONMENT
$MAKE check 2> /dev/null
if  test $? != 0; then
	echo "error during make check"
	exit 1;
fi

exit 0
