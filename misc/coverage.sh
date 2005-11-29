#!/bin/sh

# build coverage output
# require lcov installed

set -e

# go to main distro dir
DIR=`dirname $0`
cd "$DIR/.."

# cleanup
rm -rf covtmp coverage
mkdir covtmp
mkdir coverage

# move required files in covtmp
find include/ src/ \( -name \*.\[ch\] -o -name \*.da -o -name \*.bb\* -o -name \*.gc\* \) | tar cf -  -T - | (cd covtmp; tar xvf -)

# prepare
cd covtmp
find -name \*.bb\* | grep .libs | xargs rm -f
find -name \*.da -o -name \*.gc\* | grep '/.libs/' | xargs -imao -n1 sh -c 'mv mao $(echo mao | sed s,/.libs/,/,g)'

# generate coverage
geninfo . -o out.info -t 'Test'
genhtml out.info -t 'FreeTDS coverage' -o ../coverage

echo Success !!
