FreeTDS Installation
====

If you are installing from git repository please read [INSTALL.GIT](./INSTALL.GIT.md)

For complete instructions, see the [FreeTDS Users Guide](http://www.freetds.org/userguide/). It is included in the distribution inside "doc/userguide/index.html`.

Brief Instructions
----

```
$ ./configure --help
$ ./configure
$ make
$ make install
```

Post Install
====

* Edit the `/usr/local/etc/freetds.conf` file to add servers.
* Test the connection with `/usr/local/bin/tsql`
* Build and test the package that requires FreeTDS.

If you want to build the ODBC driver, you'll probably first want to install
a Driver Manager (on Unix-like systems).

For any other information please see the Users Guide.
