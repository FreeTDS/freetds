#!/bin/bash

# build coverage output
# require lcov installed

set -e
set -o pipefail

trap 'echo Error at line $LINENO' ERR

STARTDIR="$(pwd -P)"

# first parameter are gcov prefix
COV=
if [ "$1" != "" ]; then
	cd "$1"
	COV="$(pwd -P)"
	cd - > /dev/null
fi

# second is file output for info files, instead of coverage directory
OUT="$2"

# go to main distro dir
DIR=`dirname $0`
cd "$DIR/.."
DIR="$(pwd -P)"
cd - > /dev/null

if [ "$COV" = "" ]; then
	COV="$DIR"
fi

# cleanup
cd "$COV"
rm -rf covtmp coverage
mkdir covtmp
mkdir coverage

# move required files in covtmp
SRC="$(find "$COV$HOME" -name src -type d)" || true
if [ -d "$SRC" ]; then
	cd "$SRC/.."
	mkdir -p include
	find include/ src/ \( -name \*.\[ch\] -o -name \*.gc\* \) | tar cf -  -T - | (cd $COV/covtmp && exec tar xf -)
	cd "$DIR"
	find include/ src/ \( -name \*.\[ch\] -o -name \*.gcno \) | tar cf -  -T - | (cd $COV/covtmp && exec tar xf -)
else
	cd "$DIR"
	find include/ src/ \( -name \*.\[ch\] -o -name \*.gc\* \) | tar cf -  -T - | (cd $COV/covtmp && exec tar xf -)
fi

# generate coverage
cd $COV/covtmp
geninfo . -o out0.info -t 'Test'
perl -ne '$skip = 1 if (m(^SF:/usr/include/)); print if(!$skip); $skip = 0 if (/^end_of_record$/);' < out0.info > out1.info
perl -pe "s,^SF:$COV/covtmp/,SF:/home/user/freetds/," < out1.info > out.info
if [ "$OUT" != "" ]; then
	cd "$STARTDIR"
	cp $COV/covtmp/out.info "$OUT"
else
	# save temporary directory
	cd ..
	tar zcvf $HOME/coverage-$(date +%Y%m%d%H%M).tgz covtmp
	cd covtmp

	genhtml out.info -t 'FreeTDS coverage' -o ../coverage -p "$COV/covtmp"
fi

echo Success !!
