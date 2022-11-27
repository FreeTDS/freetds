Building FreeTDS from git
====

Author: James Cameron <james.cameron@compaq.com>

The git repository is maintained on GitHub. Follow these steps:

1. satisfy build dependencies, ensure that the following packages are
   installed:

	```
	automake	(GNU Automake, generates Makefile.in from Makefile.am)
	autoconf	(GNU Autoconf, generates configure from configure.ac)
	libtool		(GNU Libtool, library creation support scripts)
	make		(GNU or BSD Make.)
	gcc		(GNU Compiler Collection, for C code compilation)
	perl		(Perl, used to generate some files)
	gperf		(GNU Perf, used to generate some files)
	```

	Autotool versions that work:
	```	
	$ (autoconf --version; automake --version; libtool --version) |grep GNU
	autoconf (GNU Autoconf) 2.60
	automake (GNU automake) 1.9.6
	ltmain.sh (GNU libtool) 1.5.18 (1.1220.2.245 2005/05/16 08:55:27)
	```
	
	The above are used to generate the distributions.  
	You may get away with older versions, as far back as 2.53 for autoconf.
	
	In order to be able to compile documentation you need docbook dssl
	files. For Debian distros for instance you need xmlto and
	docbook-style-xsl packages installed (other distros varies).

2. Run `sh ./autogen.sh` to run automake, autoconf and configure,

   Any switches provided to autogen.sh will be passed to the configure script.   

3. compile the source using `make`

4. Install with `sudo make install`
