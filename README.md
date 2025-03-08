README for FreeTDS 1.6.dev (development)
====

*Release date TBD*

**To build FreeTDS read the file [INSTALL](./INSTALL.md) or
the [FreeTDS Users Guide](http://www.freetds.org/userguide/)**

FreeTDS is a free implementation of Sybase's DB-Library, CT-Library,
and ODBC libraries. FreeTDS builds and runs on every flavor of
unix-like systems we've heard of (and some we haven't) as well as
Win32 (with or without Cygwin), VMS, and Mac OS X.  Failure to build
on your system is probably considered a bug.  It has C language
bindings, and works also with Perl and PHP, among others.

FreeTDS is licensed under the GNU LGPL license. See [COPYING_LIB.txt](./COPYING_LIB.txt) for
details.

Other files you might want to peruse:

* [AUTHORS](./AUTHORS.md)  Who's involved
* [NEWS](./NEWS.md)        Summary of feature changes and fixes
* [TODO](./TODO.md)        The roadmap, such as it is

Also, [api_status](./doc/api_status.txt) shows which functions are implemented.

For details on what's new in this version, see NEWS.  For unbearable
detail, see git log.

Documentation
=============

A User Guide, in XML and HTML form, is included in this distribution.
Also included is a reference manual, generated in HTML with Doxygen.
"make install" installs the HTML documentation, by default to
/usr/local/share/doc/freetds-<version>.


Note to Users
=============

Submissions of test programs (self-contained programs that demonstrate
functionality or problems) are greatly appreciated.  They should
create any tables needed (since we obviously don't have access to your
database) and populate them.  Unit tests for any of the libraries
is appreciated

Notes to Developers
===================

The code is split into several pieces.

1. `tds` directory is the wire level stuff, it should be independent of
   the library using it, this will allow db-lib, ct-lib, and ODBC to
   sit on top.

2. `db-lib` directory. This is the actual db-lib code which runs on top of
   tds.

3. `ct-lib` directory. This is the ct-lib code which runs on top of tds.

4. `server` directory. This will be a set of server routines basically
   to impersonate a dataserver, functions like send_login_ack() etc...

5. `odbc` directory. ODBC implementation over tds.  Uses iODBC or
   unixODBC as a driver manager.  You need to have one of those if you
   are using the ODBC CLI.

6. `unittests` directories. Test harness code for ct-lib, db-lib, ODBC and
   libtds.

7. `samples` directories. Sample code for getting started with Perl,
   PHP, etc...

8. `pool` directory. A connection pooling server for TDS.  Useful if you
   have a connection limited license.  Needs some hacking to get
   configured but is quite stable once configured correctly. Contact
   the list if interested in how to use it.

Please look at doc/getting_started.txt for a description of what is
going on in the code.
