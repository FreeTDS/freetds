#!/bin/sh

# This script test that Makefiles are able to produre a good 
# distribution
#
# It create a test-dist.log file in the mail directory, you
# can tail this file to see the progress

# stop on errors
set -e

# do not create logs so diskcheck test do not fails
unset TDSDUMP || true

# set correct directory
DIR=`dirname $0`
cd "$DIR/.."

# remove old distributions
touch freetds-dummy
find freetds-* -type d ! -perm -200 -exec chmod u+w {} ';'
rm -rf freetds-*

# save directory
ORIGDIR="$PWD"

# init log
LOG="$PWD/test-dist.log"
rm -f "$LOG"
echo "log started" >> "$LOG"

# make distribution
test -f Makefile || sh autogen.sh
make dist
echo "make distribution ok" >> "$LOG"

# untar to test it, should already contains documentation
DIR=`echo freetds-* | sed s,.tar.gz$,,g`
gunzip -dc freetds-*.tar.gz | tar xf -
test -d "$DIR"
cd "$DIR"
echo "untar ok" >> "$LOG"

# assure you do not compile documentation again
mkdir fakebin
PATH="$PWD/fakebin:$PATH"
export PATH
cd fakebin
echo "#!/bin/sh
exit 1" > openjade
cp openjade doxygen
# gawk it's used by txt2man
cp openjade gawk
cp openjade autoheader
chmod +x openjade doxygen gawk autoheader
cd ..
if ! openjade --help; then true; else echo 'succedeed ?'; false; fi
if ! doxygen --help; then true; else echo 'succeeded ?'; false; fi
if ! gawk --help; then true; else echo 'succeeded ?'; false; fi
if ! autoheader --help; then true; else echo 'succeeded ?'; false; fi
echo "fakebin ok" >> "$LOG"

# direct make install (without make all)
mkdir install
INSTALLDIR="$PWD/install"
mkdir build
cd build
# --enable-msdblib --enable-sybase-compat can cause also problems, try to compile with both
../configure --prefix="$INSTALLDIR" --enable-msdblib --enable-sybase-compat --disable-libiconv
# make clean should not cause problems here
make clean
make install
cd ..
echo "direct make install ok" >> "$LOG"

# cleanup
rm -rf install build

# again with dist and autogen
mkdir build
cd build
../autogen.sh
make dist
cd ..
rm -rf build
echo "make dist ok" >> "$LOG"

# test if make clean clean too much
mkdir install
./configure --prefix="$PWD/install"
make clean
make dist

# finally big test. I hope you have a fast machine :)
cd ..
rm -rf "$DIR"
gunzip -dc freetds-*.tar.gz | tar xf -
cd "$DIR"
./configure
if test ! -e PWD -a -e "$ORIGDIR/../PWD"; then
	cp "$ORIGDIR/../PWD" .
fi
if test ! -e PWD -a -e "$ORIGDIR/PWD"; then
	cp "$ORIGDIR/PWD" .
fi
make distcheck
echo "make distcheck ok" >> "$LOG"

# cleanup
cd "$ORIGDIR"
chmod -R 777 "$DIR"
rm -rf "$DIR"

# check rpm
RPMCMD=rpm
if rpmbuild --help > /dev/null 2>&1; then
	RPMCMD=rpmbuild
fi
if $RPMCMD --help > /dev/null 2>&1; then
	$RPMCMD -ta freetds-*.tar.gz || exit 1
	echo "rpm test ok" >> "$LOG"
else
	echo "rpm test skipped, no rpm detected" >> "$LOG"
fi

echo "all tests ok!!!" >> "$LOG"

