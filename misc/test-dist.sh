#!/bin/sh

# stop on errors
set -e

# set correct directory
DIR=`dirname $0`
cd "$DIR/.."

# remove old distributions
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
tar zxf freetds-*.tar.gz
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
chmod +x openjade doxygen
cd ..
if ! openjade --help; then true; else echo 'succedeed ?'; false; fi
if ! doxygen --help; then true; else echo 'succeeded ?'; false; fi
echo "fakebin ok" >> "$LOG"

# direct make install (without make all)
mkdir install
INSTALLDIR="$PWD/install"
mkdir build
cd build
../configure --prefix="$INSTALLDIR"
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

# finally big test. I hope you have a fast machine :)
cd ..
rm -rf "$DIR"
tar zxf freetds-*.tar.gz
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

