$Id$

Summary of Changes in release 1.5
--------------------------------------------
User visible (not in a particular order):
- Generic:
  - Do not write to stdout/stderr;
  - (\*) Fix certificate check for hostnames with wildcards;
  - (\*) Update to Autoconf 2.71;
  - (\*) Fix conversions of "-0" numeric;
  - (\*) Minor compatibility for 32 bit for bulk copy;
  - Distribute OpenSSL libraries with Appveyour artifacts;
  - Use Unicode and wide characters for file paths for Windows;
  - Remove ANSI encoding code from SSPI code, always full Unicode;
  - (\*) Set control method for final OpenSSL BIO to avoid some errors;
  - (\*) Get Windows code page using Windows API for compatibility;
  - (\*) Do not try to use `pthread_cond_timedwait_relative_np` on newer Android;
  - Fix compatibility connecting to some old MSSQL 2000 server;
  - (\*) Support Sybase server not configured with UTF-8 charset;
  - (\*) Change some file license from GPL to LGPL;
  - (\*) Support very old Sybase ASE versions;
  - (\*) Ignore query errors during connection initial setup increase
    compatibility with different servers like OpenServer;
  - Support `strict` encryption for naked TLS (TDS 8.0);
  - (\*) Don't leak allocations on syntax errors converting to binary;
  - Accommodate FreeBSD/Citrus iconv;
  - Fall back on unsupported address families locking for host address;
  - Avoid potential hangs on short replies reading TDS packets;
  - Add support for TDS 8.0;
  - Reject invalid NULL data in `tds7_send_record` (bulk transfer);
  - Allows `freetds.conf` to be stored in `~/.config` (Unix).
- ODBC:
  - Fixed some attribute size for 64 bit platforms (now more compatible
    with MS driver);
  - (\*) Prevent setting some wrong type in internal decriptors;
  - (\*) Fix leak in `odbc_parse_connect_string`;
  - Fix an issue compiling bcp test on 32 systems with unixODBC;
  - (\*) Fix getting `SQL_ATTR_METADATA_ID` attribute;
  - Check for maximum value for `SQL_ATTR_QUERY_TIMEOUT`;
  - (\*) Return better error for invalid character set;
  - Check errors from `SQLInstallDriverExW` and `SQLRemoveDriver`;
  - Update some driver registration field for Windows;
  - (\*) Remove minor leak parsing connection string;
  - Add `Encrypt` and `HostNameInCertificate` settings;
  - (\*) Fix bug cancelling not active statements;
  - Fix `SQLGetInfo` `SQL_DRIVER_HSTMT` and `SQL_DRIVER_HDESC`;
  - Avoid shared object version for ODBC driver;
  - Allows `SERVER` to override `DSN` or `SERVERNAME` settings;
  - Allows to set version to `AUTO` from Windows dialog;
  - (\*) Fix handling of `SQL_C_STINYINT`;
  - Fix return value when bulk operation not using status array fails;
  - Implement `SQLDescribeParam` using `sp_describe_undeclared_parameters`.
- Applications:
  - datacopy:
    - Allows copy when `dest_collen` > `src_collen`;
    - Increase CREATE TABLE command buffer to accommodate larger queries.
  - defncopy:
    - (\*) Use memory instead of temporary file;
    - (\*) Fix MS column length for N(VAR)CHAR types;
    - (\*) Quote strings and identifiers;
    - (\*) Handle correctly order of index recordset;
    - (\*) Quote key index names.
- CT-Library:
  - Error reporting more compatible with Sybase;
  - Return CT-Library type values, not TDS ones;
  - Fix `*resultlen` for conversions to SYBIMAGE from `cs_convert`;
  - Populate `datafmt->format` to avoid not initialized values;
  - More debugging on not implemented `bcp_colfmt_ps`;
  - Makes sure we don't use a negative number as string length;
  - Use client type, not propagate TDS one;
  - (\*) Fix crash using `ct_command` with `CS_MORE` option;
  - (\*) `_blk_get_col_data`: Consistently return `TDS_FAIL` on failure;
  - (\*) Formally define `BLK_VERSION_{155,157,160}`;
  - (\*) Cap binary/image copying to the destination length;
  - (\*) Conditionally distinguish NULL and empty results;
  - (\*) Issue an error for unsupported server types from `ct_describe`;
  - Add support for setting hints for bulk copy using `blk_props`;
  - Support getting `CS_ENDPOINT` (socket file descriptor) using `ct_con_props`;
  - Support getting `CS_PRODUCT_NAME` using `ct_con_props`;
  - Introduce `CS_INTERRUPT_CB` and corresponding return values: `CS_INT_*`;
  - Better support new date/time types (DATETIME2, etc.) in bulk copy;
  - Report system errors' descriptions.
- DB-library:
  - Improve error reporting;
  - Allows to set port number with `DBSETLPORT`;
  - Allows encryption option.
- pool:
  - Disable Nagle algorithm on user socket for performance;
  - (\*) Ignore extension in login packet for compatibility.
- server:
  - Reply correct version for TDS 7.4;
  - Avoid leaks in `tds_get_query_head`;
  - Set `SO_REUSEADDR` option;
  - Better support for string conversion and UTF-8;
  - Improve sending data, fix sending metadata;
  - Update prelogin reply allowing TDS 7.2;
  - Fix `tds_send_login_ack` product name length;
  - Support no-ASCII characters in environment names;
  - Return 0 rows during login;
  - Pass correct packet size environment in test server;
  - Do not overwrite error before displaying it;
  - Improve message writing functions;
  - Set `TCP_NODELAY` after accepting connected sockets.

(\*) Feature backported in stable 1.4 branch.

Implementation:
- Remove various minor leaks;
- Remove some potential `NULL` dereference;
- (\*) Add support for `SYBSINT1` type conversion;
- Improve GNU compatibility in CMake using `GNUInstallDirs`;
- Remove many warnings compiling code with stricter options;
- Optimize numeric precision change;
- Use more `bool` type for boolean instead of integer;
- Minor compatibility with tests and Sybase/SAP libraries;
- Avoid some warnings from CMake;
- Various improvements to bounce utility:
  - Allows multiple connections;
  - Allows to specify a server name;
  - Allows to write dumps.
- Use 64 bit constants, add `(U)INT64_C` macros;
- Move `tds_new` macros to `include/freetds/macros.h`;
- Move `tds_strndup` to utils;
- Add GitHub actions to CI;
- Allows `FREETDS_SRCDIR` overrides for tests;
- Avoid potential zero-byte allocations (whose behavior is undefined)
  in libTDS;
- Unify header guards definitions;
- Unify tests includes in ctlib;
- Accommodate Windows static builds;
- Inform CMake of some accidentally Autotools-only tests;
- Add Visual Studio 2022 to Appveyor test matrix;
- Add `tds_socket_set_nodelay` utility;
- Change `TDSCOLUMN::column_bindlen` field to signed type;
- Use `TDS server` instead of `Adaptive Server` in error messages;
- Add and reuse `TDS_END_LEN_STRING` utility;
- Acknowledge non-exhaustive `TDS_SERVER_TYPE` switch statements;
- Use replacement `getopt` in tests for more compatibility;
- Include `stdint.h` in Visual Studio if available.

Summary of Changes in release 1.4
--------------------------------------------
User visible (not in a particular order):
- Fix some numeric conversion checks;
- Always use Unicode for SSPI allowing not ASCII to work;
- Improve BCP copy, especially for Sybase;
- Better error reporting for ICONV failures;
- Disable TLSv1 by default;
- ODBC: partial TVP support (missing data at execution);
- ODBC: support for quoted string in connection string;
- CT-Library: support large identifiers;
- CT-Library: report appropriate severity values;
- apps: datacopy report errors on standard error;
- pool: use poll instead of select to support more connections.

Implementation:
- Use more bool type for boolean instead of integer;
- more macros for ODBC tests to encapsulate some ODBC API.

Summary of Changes in release 1.3
--------------------------------------------
User visible (not in a particular order):
- Generic:
  - Support UTF-8 columns using MSSQL 2019;
  - Do not accept TDS protocol versions "4.6" (never really supported) and
    "8.0";
  - Minor portability issues;
  - Fix log elision for login;
  - Detect some possible minor memory failure in application;
  - Support long (more than 64k) SSPI packets (never encountered but you
    never know);
  - Fix unicode columns for ASA database;
  - Avoid using BCP with old protocols;
  - (\*) Fix bulk copy using big endian machines;
  - (\*) Fix Sybase uni(var)char and unsigned types for big endian machines;
  - (\*) Do not send nullable data during bulk copy if type is not nullable;
- ODBC:
  - Added "Timeout" setting;
- Applications:
  - Improve defncopy utility:
    - Fix some declaration;
    - Fix Sybase support;
  - (\*) Fix datacopy and freebcp logging ;
- CT-Library:
  - Minor fix for variant type;
  - Better support for timeout setting;
  - (\*) Support some missing types (like nullable unsigned integers) for
    Sybase;
- DB-library:
  - Unify date format (all systems can use the same syntax);
  - (\*) Allows to pass 0 as type for bcp_bind;
  - (\*) Fix DBSETLSERVERPRINCIPAL macro;
  - (\*) Do not limit queries length for bcp using Sybase;
  - (\*) Add KEEP_NULLS to BCP hints.

(\*) Feature backported in stable 1.2 branch.

Implementation:
- Move replacement headers under freetds directory for coherence with other
  internal headers;
- Lot of style updates;
- Optimize UTF-8 encoding for ODBC, reuse common code.

Summary of Changes in release 1.2
--------------------------------------------
User visible (not in a particular order):
- Sybase server:
  - All strings are now converted as MSSQL;
  - Support kerberos login;
  - DB-Library: add DBSETNETWORKAUTH, DBSETMUTUALAUTH, DBSETDELEGATION and
    DBSETSERVERPRINCIPAL;
  - CT-Library: add CS_SEC_NETWORKAUTH, CS_SEC_NETWORKAUTH,
    CS_SEC_NETWORKAUTH and CS_SEC_NETWORKAUTH;
- Bulk copies:
  - DB-Library: fix trim of unicode fields;
  - (\*) Apply character conversion for Sybase, like MSSQL;
  - (\*) Ignore computed columns;
  - (\*) Properly support multibyte strings in column names;
  - (\*) DB-Library: stop correctly on BCPMAXERRS setting;
  - (\*) DB-Library: do not try to convert skipped rows reading file allowing
    for instance to load CVS files;
- (\*) CT-Library: added CS_DATABASE property to allows to connect correctly
  to Azure servers;
- (\*) Improve support for MS XML columns for both DB-Library and CT-Library;
- (\*) Fix some issues with MSSQL server redirection (used for instance in
  Azure);
- (\*) Change SQL_DESC_OCTET_LENGTH value for wise character columns;
- Better support for SQL_VARIANT:
  - Better column checks;
  - (\*) CT-Library: now supported, columns are returned as CS_CHAR_TYPE;
- Some updates to server part:
  - Set correctly initial state;
  - IPv6 support;
  - Fix TDS 7.2 logins;
- Support extended character using domain logins under Unix;
- Improve MARS:
  - Less memory copies;
  - (\*) Remove possible deadlock;
  - Handle wrapping sequence/window numbers;
  - Make sure we sent the wanted packet;
- (\*) Support UTF-16 surrogate pairs in odbc_wide2utf and odbc_set_string_flag
  fixing some character encoding support;
- (\*) Fix multiple queries, used by ODBC to optimize data load;
- (\*) Improve emulated parameter queries, fixing minor issues and reducing
  memory usage;
- Support DBVERSION_UNKNOWN in dbsetlversion (will use automatic detection);
- CT-Library: define CS_MIN_SYBTYPE and CS_MAX_SYBTYPE;
- (\*) CT-Library: fix cs_will_convert accepting library constants, not libTDS.

(\*) Feature backported in stable 1.1 branch.

Implementation:
- User guide converted to XML;
- Always use little endian on network level, simpler and faster;
- Converted some files to markdown;
- Optimize bytes endianess conversion;
- Made character lookup faster making character conversion quicker;
- Optimize data padding in CT-Library;
- Fix some build dependencies causing some files to not be properly
  recompiled during development;
- Add "freeze" support allowing to pause sending in order to simplify
  lengths computations sending data.

Summary of Changes in release 1.1
--------------------------------------------
User visible (not in a particular order):
- Changed default TDS protocol version during configure to "auto".
  Versions 4.2 and 7.0 are no longer accepted for default, you may
  still specify an explicit version to connect to obsolete servers
  ("auto" won't attempt these versions);
- allows to disable TLS 1.0 support;
- pool server allows to specify different username/password for server
  and clients allowing to hide internal server logins;
- tsql utility now send final partial query to server to avoid to have
  to specify a final "GO" line to terminate commands;
- better support for Microsoft cluster, client will attempt multiple
  connection to server at the same time if DNS reply multiple IPs;
- MONEY/SMALLMONEY types are now formated with 4 decimal digits to
  avoid truncation;
- MARS support is now compiled by default;
- pool server is now compiled by default;
- Fixed SQL_ATTR_LOGIN_TIMEOUT for ODBC;
- Fixed large integer numbers for ODBC RPC constants;
- Fixed encrypted logins if "auto" protocol version is used;
- Support CS_TIMEOUT, CS_LOGIN_TIMEOUT and CS_CLIENTCHARSET properties
  under CTLibrary;
- Added a dbacolname function in DBLibrary, similar to dbcolname but
  for compute columns (mainly for Sybase now, Microsoft removed
  support for compute columns);
- NTLMv2 is now on by default.

Implementation:
- Improved UTF-8 performances;
- Use more stdint.h style definitions (like uint32_t);
- Use bool type instead of int;
- pool server compile under Windows too (not as capable as Unix
  version);
- CMake build is now able to do an installation;
- Added a src/utils directory to collect all common code not strictly
  related to replacements or TDS;
- Simplified makefiles;
- Support CP1252 encoding in internal trivial iconv;
- Better ODBC detection. Allows to specify a directory with
  --with-iodbc to specify custom iOBDC.

Summary of Changes in release 1.0
--------------------------------------------
User visible (not in a particular order):
- Removed "8.0" from protocol version string accepted. Please
  update configuration files;
- Default protocol version is now auto. This could slow down
  connection but make user experience less painful;
- Sybase encrypted login. Set encryption to get it;
- Support protocol version 7.4;
- Add intent support to specify we don't want to change data;
- Allow to attach database file during the login (MS SQL Server);
- Support for Sybase time/date/bigdate/bigdatetime;
- Pool is working again;
- ODBC BCP (not complete);
- Improved dbconvert and dbconvert_ps (more compatible);
- Fixed dbspid;
- Improved ODBC type information;
- Better certificate verification;
- AppVeyor is used for every build;
- Try all IPs from DNS. This allows SQL Cluster connection
  to secondary servers.

Implementation:
- Removed Nmake support;
- Type conversions simplified;
- Better type handle code.

Summary of Changes in release 0.95
--------------------------------------------
User visible (not in a particular order):
- Support for build with CMake under Windows.
- Support MSSQL 2008:
  - new date/time types;
  - NBCROWs;
  - able to retrieve CLR UDT types in binary format.
- Moved from CVS to git.
- Support MARS under ODBC.
- IPv6 support.
- Support unsigned Sybase types.
- Sybase use some characters conversion like MSSQL.
- Bulk-copy improvements:
  - more types;
  - support for empty fields;
  - no needs for seekable file;
  - avoid possible buffer overflows;
  - less memory consumption;
  - handle different BIT fields in Sybase;
  - -T option for datacopy.
- datacopy:
  - prompting for password (use "-" password on command line);
  - empty user/password (to use Kerberos).
- Support for query notifications in ODBC.
- Support for protocols 7.2 and 7.3 under dblib and ctlib.
- dblib:
  - dbpivot extension;
  - dbprcollen extension to get printable column size;
  - add DBCOL2 structure for dbtablecolinfo;
  - support DBTEXTSIZE option for dbsetopt;
  - support SRCNUMERICBIND and SRCDECIMALBIND binding;
  - DATETIME2BIND extension binding to support new mssql 2008 type;
-  ODBC:
  - add SQL_INFO_FREETDS_TDS_VERSION option to SQLGetInfo to retrieve FreeTDS
    version;
  - add --enable-odbc-wide-tests configure option to use wide functions in
    unit tests.
- Better thread support.
- Allow to specify server SPN for GSSAPI.
- Sybase graceful disconnects.
- Better support for VARCHAR(MAX)/VARBINARY(MAX).
- Better error reporting from login failure.
- More strict test for configuration errors, fails if configuration is wrong.
- "use utf-16" option to enable use of UTF-16 for server encoding (MSSQL).
- Remove support for Dev-C++.
- Remove support for bad iconv, use libiconv instead (Tru64 and HP-UX should
  be affected), connect will fail.
- New NTLM options:
  - "use lanman";
  - "use ntlmv2".
- New Kerberos options:
  - "realm" ("REALM" in ODBC);
  - "spn" ("ServerSPN" in ODBC).
- New certificate options:
 - "ca file";
 - "crl file";
 - "check certificate hostname".
- Many bug fixes, majors:
  - SSPI with Kerberos on some setup;
  - SQLCancel threading;
  - ctlib numeric;
  - ODBC statistic function quotings.

Implementation:
- Introdude streams support to make easier handling large data and
  making conversions faster.
- Use array of flags to make faster to retrieve data type features.
- Use callbacks for handling various type functions.
- Optimize data conversions.
- Store TDS packets in a new structure to handle multiple packets.
- New DSTR implementation which use less memory.
- Faster logging.
- Put mostly data handling in src/tds/data.c.
- Move mostly headers to include/freetds directory.
- Add a include/freetds/proto.h header to separate protocol declarations.
- Add src/tds/packet.c to separate code handling TDS packets.
- Modify libTDS error codes, not only TDS_FAIL.
- Use DSTR for names in TDSCOLUMN.
- Generate graphs in Doxygen.

Summary of Changes in release 0.91
--------------------------------------------
1.  Full Kerberos and SSPI support for passwordless login to
    Microsoft SQL Server from Unix and Windows clients.
    Includes Kerberos delegation option.
2.  Full support for DB-Library under Win32/64 via NMAKE.EXE.
3.  Built-in support for UTF-8.
4.  Support for wide characters in ODBC.
5.  Support for varchar(max) and varbinary(max).
6.  Better thread-safety in ODBC.
7.  Distinguish between connect and login errors.
8.  Bulk-copy functions in CT-Library.

TDS versions now reflect Microsoft's nomenclature.  The previous
version numbers (8.0 and 9.0) are now 7.1 and 7.2.  See the UG
for details.


Executive Summary of Changes in release 0.82
--------------------------------------------

1.  timeout handling
2.  encrypted connections
3.  fisql (and odbc utilities)
4.  autoconf improvements
5.  23,710 lines added or deleted (101,022 total).
6.  85 files added
7.  21 unit tests added

Details
-------

db-lib
- timeouts work!
- corrected dbnextrow
- implemented dbsetnull and dbsetinterrupt
- improved error reporting and checking
- fixed rpc parameter processing, now php works correctly

ct-lib
- added cs_loc_alloc, cs_loc_drop, cs_locale implementations

odbc
- cursors (mssql)
- fixed database setting
- return error always if odbc returns SQL_ERROR
- fixed SQLGetData result

utilities
- added support for NUL characters inside terminators in freebcp
- added row termination and column termination option to tsql
- new fisql application
- new ODBC utilities

documentation
- significant updates to TDS protocol documentation
- freetds.conf man page
- added tenderfoot sample code

general
- fixed timeout handling
- added freetds.conf option for encryption
- added protocol version discovery
- NTLM2 session response
- read table and real column name from wire
- experimental Kerberos support using gssapi
- some optimizations for GCC4
- optimized conversions avoiding some memory copy
- minor improves to server stuff
- improved MingW compile (even cross one)
- more verbose log for dblib and odbc
- many test added
  1 test for libTDS
  1 test for ctlib
  3 tests for dblib
  13 tests for odbc

libTDS API changes
- tds_add_row_column_size removed
- tds_alloc_row return now TDS_SUCCEED/TDS_FAIL
- tds_alloc_compute_row return now TDS_SUCCEED/TDS_FAIL
- removed TDSCOLUMN->column_offset
- added TDSCOLUMN->column_data and TDSCOLUMN->column_data_free
- added TDSCURSOR->type and TDSCURSOR->concurrency for mssql support
- added fetch_type and i_row parameters to tds_cursor_fetch
- added tds_cursor_update and tds_cursor_setname functions
- made tds_alloc_get_string static
- removed tds_free_cursor
- added TDSCURSOR->ref_count
- added tds_cursor_deallocated and tds_release_cursor to handle
  cursor release. tds_cursor_deallocated is called when cursor got
  deallocated from server while tds_release_cursor is called to
  decrement reference counter. Reference counter is used cause is difficult
  to trace pointer owner between libTDS and upper libraries
- added TDS_COMPILETIME_SETTINGS->sysconfdir
- changed DSTR_STRUCT structure to include dstr_size
- changed DSTR type
- error handler cannot return TDS_INT_EXIT
- removed TDSSOCKET->query_timeout_func TDSSOCKET->query_timeout_param,
  TDSSOCKET->query_start_time
- changed TDSLOGIN->host_name to client_host_name
- changed TDSCONNECTION->host_name to client_host_name
- changed TDSLOGIN->encrypted to encryption_level
- changed TDSCONNECTION->encrypted to encryption_level
- added TDSRESULTINFO->row_free handler to free row
- added TDSCONTEXT->int_handler handler
- removed tds_prtype
- removed tds_alloc_param_row
- added tds_alloc_param_data
- added flags parameter to tds7_send_auth
- removed tds_client_msg
- added tdserror to report error
- added flags parameter to tds_answer_challenge
- added is_variable_type macro
- added TDSCOLUMN->table_column_name
- added tds parameter to tds_set_column_type
- renamed BCPCOLDATA->null_column to is_null
- added TDSMESSAGE->oserr
- remove length, number_upd_cols and cur_col_list from TDSCURSOR (never used)
- added new TDSAUTHENTICATION to handle authentication modules
- added TDSSOCKET->authentication
- added tds_ntlm_get_auth and tds_gss_get_auth
- removed TDSANSWER
- removed tds7_send_auth (use new tds_ntlm_get_auth)
- added tds_get_int8
- add TDSPARAMINFO *params argument to tds_cursor_declare
- add TDSPARAMINFO *params argument to tds_cursor_open
- changed TDSSOCKET->rows_affected from int to TDS_INT8
- added TDSSOCKET->tds9_transaction (used internally for TDS9)
- added TDSCONNECTION->server_host_name needed for Kerberos support

* 0.64
- core library
 - reduced network bandwidth use on Linux and *BSD
 - do not free TDSSOCKET in tds_connect
 - moved network stuff into net.c
 - fixed conversion NUMERIC->NUMERIC changing precision/scale
 - added named instance support (mssql2k)
 - fixed cancel and timeout
 - added support for encrypted connection using mssql
   (using either GnuTLS or OpenSSL)
 - improved numeric conversions performance
 - improved debug logging (added "debug flags" option)
- ctlib
 - ct_dynamic and friends (placeholder support)
- dblib
 - more functions
 - made threadsafe
 - improved bcp
 - support for large files using BCP
 - fixed buffering
- ODBC
 - fixed compute handling in ODBC
 - paramset support
 - constant parameters in rpc (ie {?=call func(10,?)} )
 - configure use automatically odbc_config if available
- compatibility
 - improve PHP support
 - improve DBD::ODBC support
 - partial dos32 support
 - improve JDBC support
 - added msvc6 project to build dblib library on windows
- support long password on tsql for all platforms
- improved pool server
- RPMs
 - ODBC driver registration (in odbcinst.ini)
 - better dependency for RedHat and SUSE
- a lot of fixes

libTDS API changes
- tds_connect does not free TDSSOCKET* on failure
- TDSSOCKET->env is not a pointer anymore
- tds_free_compute_result and tds_free_compute_results are now static
- added TDSDYNAMIC->next for linked list
- removed TDSCURSOR->client_cursor_id
- use TDSCURSOR* cursor instead of TDS_INT client_cursor_it in
  tds_cursor_* functions
- added const char* file param to tdsdump_dump_buf and tdsdump_log and
  added line information in level
- changed tds_alloc_compute_results declaration, pass a TDSSOCKET*
- removed tds_do_until_done (not used)
- removed tds_set_longquery_handler function (not used)
- changed some fields in TDSSOCKET
 - removed out_len (not used)
 - renamed cursor to cursors
 - removed client_cursor_id
 - added cur_cursor (instead of removed client_cursor_id)
 - dyns now a pointer to a pointer to first dynamic allocated
 - removed num_dyns (now useless with linked list)
 - removed chkintr and hndlintr (use longquery_*)
 - removed longquery_timeout (use query_timeout)
 - renamed longquery_func to query_timeout_func
 - renamed longquery_param to query_timeout_param
 - queryStarttime to query_start_time
- add a DSTR instance_name to TDSCONNECTION
- add tds7_get_instance_port
- removed tds_free_all_dynamic
- tdsdump_append now static
- new tds_process_tokens to replace removed
 - tds_process_result_tokens
 - tds_process_row_tokens
 - tds_process_row_tokens_ct
 - tds_process_trailing_tokens
- removed tds_ctx_set_parent/tds_ctx_get_parent, use parent member
- added void * parent argument to tds_alloc_context
- tds_process_tokens return a TDS_CANCELLED if handle cancellation
- removed query_timeout_func and query_timeout_param from
  TDSCONNECTION and TDSLOGIN
- add tds_free_row
- renamed TDSLOCALE->char_set to server_charset

* 0.63
- ODBC: use tds_dstr* functions to store descriptor information
- header privatizations (removing tds.h dependency).  This is quite
  important for future binary compatibility.
- ODBC: SQLFetch returns error correctly
- ODBC: fix problem rebinding parameters
- ODBC: ability to fetch data types after prepare (needed for Oracle
  bindings and OTL library).
- Builds cleanly under OS X.
- Improved BCP support for NULL fields and native file format.
- ct-lib: ct_blk support (bcp for ct-lib).
- ct-lib: Cursors!
- apps: added bsqldb and defncopy.
- iconv: better collation support, e.g. SQL_Scandinavian_CP850_CI_AS

* 0.62
- ct-lib: cursor support
- fixed PHP problem handling empty recordsets. See messages on ML:
  Damian Kramer, September 23, "Possible bug in freeTDS"
  Steve Hanselman, September 16 "Issue with freetds 0.61.2 ..."
- ODBC: improved, best error report
- ct-lib: support ct_diag (for Python)
- ODBC: fixed SQLMoreResults/SQLRowCount and batch behavior
- ODBC: fixes call to {?=store(?)}
- ODBC: multiple record with
  "select id,name from sysobjects where name = 'sysobjects'"
- ODBC: fixed early binding
- ODBC: autodetect iODBC or unixODBC during configure
- ODBC: implemented option 109 in SQLGetConnectOption (for OpenOffice)
- freebcp understand \n as newline. Also \r and \0 (null byte).
- added --without-libiconv configure option to switch off iconv library
- ODBC: test and fixes for NUMERIC parameter
- ODBC: dynamic query
  - SQLPutData
  - Sybase and blobs
    ported code for string building from ODBC to libtds
- extended TDSSOCKET::iconv_info as an array.  Keep converters for non-UCS-2
  server charsets.  Every TDSCOLINFO holding character data should point to
  one of these elements.
- dblib: src/dblib/unittests/t0017.c (bcp) fixed
- changed tds_get_char_data(), ML 2 May 2003, "tds_get_char_data and iconv".
- rewrote tds_iconv:
 - use iconv() signature.
 - rely on TDSCOLINFO::iconv_info for conversion descriptor, instead
   of inferring it from the column sizes.
 - on read error, emit Msg 2403, Severity 16 (EX_INFO):
   "WARNING! Some character(s) could not be converted into client's character
   set. Unconverted bytes were changed to question marks ('?')."
 - on write error emit Msg 2402, Severity 16 (EX_USER):
   "Error converting client characters into server's character set. Some
   character(s) could not be converted."
   and return an error code.  Do not proceed.
 - Cf. ML 16 Apr 2003. "conversion error management".
- added doxygen to the nightly build
- "make install" put the UG in PREFIX/share/doc/freetds/userguide
- "make install" put doxygen html in PREFIX/share/doc/freetds/reference
- moved website docs (not UG) to doc/htdoc, put in CVS
- added bcp support to tds/dblib/ctlib. (started in dblib)
- RPC stuff
- added support for TDS 8.0
- TDS 7 Unicode to native charset conversion using iconv
- autoconf the connection pooling stuff
- DBLIB: output params
- set database during login process
- libtds: dynamic query
- ctlib: null returns zero-length string
  (see "SELECT '' and TDS 7.0" in message list on 26 Jan 2003)

* Jan 2003
- Version 0.61
- Dynamic SQL
- Output parameters
- Compute rows
- Varbinary support
- dsn-less ODBC connections
- RPC support (db-lib)
- Compatibility with DBD::Sybase 0.95
- 68 new functions!  (see doc/api_status.txt)
- Error/message handling rewritten, uses real error numbers
- new sample programs
- much cleaner code, warning-free compiles
- namespace cleanup
- public domain versions of functions for OSs that lack them
- autoconf portability improvements
- builds in HP-UX, Win32, and cygwin
- No dependency on OpenSSL
	
* Sep 2002
- Version 0.60
- Support for SQL Server 2000 datatypes and domain logins.
- Support and convertibility of all major datatypes.
- Much expanded coverage of the ODBC API.
- An all-new BCP implementation, including host variable support.
- Character set conversions, via the iconv library.
- Threadsafe operation.

* [late 2001?]
- tdspool now working for big endian systems
- Fixed mem leak in ctlib
- Added some descriptive text to the PWD file
- EINTR handling during login (Kostya Ivanov)
- Added support for TLI style interfaces files (thanks Michael for explaining)
- Added 'text size' config option which changes textsize on connect
- Added preliminary TDS 8.0 support (no new datatypes supported though)
- Added 'emulate little endian' config flag
- Some TDS5 placeholder stuff.  Not ready for primetime yet.
- Fixed interfaces handling seg fault

* Jul 2001 Brian Bruns <camber@ais.org>
- Version 0.52
- Mem leak fixes in dbloginfree() and tds_free_env() (John Dumas)
- Added support for new configuration format (freetds.conf)
- unixODBC now working
- Fixed error in two's complement function for money types
- Added support for nullable bits (BITN)
- checked in work on tds connection pooling server
- added preliminary userguide (James Lowden and me)
- a lot of work on ODBC driver, now works with PHP
- added config options for iodbc/unixodbc (unixODBC doesn't actually work yet)
- image -> char now works (verified with ctlib only)
- varbinary -> char now works with destlen of -1
- New config routines
- Free socket on login fail
- 64bit patches
- off by one error
- numeric problem with 7.0 fixed
- digit cutoff on numerics/floats fixed

* Nov 2000 Brian Bruns <camber@ais.org>
- Version 0.51
- removed all the old unittests from the samples directory
- endian detection fixed
- 'make check' and ctlib unittests (Mark)
- TDS 7.0 fixes, numerics et al. (Scott)
- dead connection handling (Geoff)
- query timeout stuff (Jeff)

* Dec 1999 Brian Bruns <camber@ais.org>
- Version 0.50
- Added TDS 7.0 support for MS SQL 7
- Added hostfile bulk copy for dblib
- Added writetext support for dblib
- Added CS_CON_STATUS property to ctlib
- Fixed bugs for ctlib version of PHP 3/4
- Many changes to text/image handling
- New script for running the unittests
- dbcancel/ct_cancel now working properly
- inserts/updates now return proper rowcount
- Numerous bug fixes

* Sep 1999 Brian Bruns <camber@ais.org>
- Version 0.47
- Added workaround for SQL 7.0 bug handling datetime/money
  for big endian (testing/bug report - Paul Schaap)
- Added TDS 7.0 login function (untested)
- Fixed many big endian bugs
- Fixed some bus errors on Sparc
- Fixed big endian detection
- DBD::Sybase 0.19 now passes all tests
- Fixed date bug working with PHP 3.x
- binary/varbinary support added
- Fixed Text datatypes in tds layer
- More conversions implemented (Mark Schaal)
- Fix make install for not overwriting interfaces (Michael Peppler)
- CS_USERDATA now works
- Numerous bug fixes (many people)

* Thu Aug xx 1999 Brian Bruns <camber@ais.org>
- Version 0.46
- Fixed floating type support
- Fixed lots of little datatype conversion bugs
- Fixed 5.0 login acknowledgement bug
- Message processing was cleaned up (Mark Schaal)
- Fixed login bug for SQL Server 7.0
- DBD::Sybase 0.19 now compiles and partially works (very partially)
- Fixed Solaris #define clash
- Numerous bug fixes

* Thu Jun 03 1999 Brian Bruns <camber@ais.org>
- Version 0.45
- Capabilities added to ctlib code
- Numeric support working
- MONEY to string conversions now support > 32 bit values
- Fixed underread in message handling
- Fixed various buffer overflow problems
- Fixed NULL handling
- Added support for length binding (copied arguement to ct_bind)
- Converted ODBC to use iODBC driver manager
- SQSH 1.7 runs
- PHP 3.0.x with ctlib now runs

* Thu Jan 14 1999 Brian Bruns <camber@ais.org>
- Version 0.41
- Better row buffering (Craig Spannring)
- CT-Lib code improved greatly
- Closer behaviour to real dblib
- Commonized datatype conversions
- Server side code is running somewhat.
- SQSH 1.6 running
- More ODBC functionality
- Many bug fixes
- Lots of other stuff I've already forgotten

* Sun Nov 22 1998 Brian Bruns <camber@ais.org>
- Version 0.40
- Row buffering is now supported for dblib.
- Better row handling (side effect of above)
- Improved conversion code
- Preliminary ODBC layer
- PHP now runs basic scripts, maybe more
- Many many bug fixes
- General cleanup (better error handling, C++ friendly headers, etc...)

* Fri Sep 04 1998 Mihai Ibanescu <misa@dntis.ro>
- Version 0.31
- By default the install dir is /usr/local/freetds
- The Makefile in the samples dir is automatically built from Makefile.am.
	The samples dir is not installed, only packaged in the distribution.

* Wed Sep 02 1998 Brian Bruns <camber@ais.org>
- Version 0.3
- Updated the AUTHORS file
- FIXME Brian (added by misa)

* Mon Aug 31 1998 Mihai Ibanescu <misa@dntis.ro>
- Version 0.21
- GNUified
- Fixed a couple of the TODO issues: byte order is automatically determined,
	and the TDS version is a configurable option
- Modified the README file to reflect the new directory structure

Pre-GNUification log by Brian <camber@ais.org>:

2/8/98	Should be able to send the first packet to a server soon, my output is
	only slightly different than open clients.
	This codes pretty crappy right now. I need to clean up alot of stuff,
	remove hardcode values, etc...but I'm anxious to see something work!
2/7/98	Broke the code up a bit, tds.c now handles all wire level stuff,
	dblib.c handles dblib specific stuff. So, in the future there can be
	ctlib.c and obdc.c can also sit on top of tds.c to handle the other CLIs
3/16/98 Been working on the code here and there...We can now send a query to
	the server, dbnumcols() and dbcolname() both work. Almost ready to get
	some data back. I put in a dummy dbbind to just handle strings so,
	I could do some work on dbnextrow().  However, we will have revisit
	almost everything later.
3/23/98	Haven't been able to work on it lately.  Still trying to decide on the
	best way to propagate the row data from tds.c to dblib.c to the calling
 	func.  Not that hard, but nothing strikes me as the "Right Way" (tm).
4/2/98	Ok we are ready to release 0.01 (marked by the fact that a simple dblib
	program actually works!)
5/1/98  Haven't updated in quite a while. A few more dblib commands are
	supported. dbconvert() support is preliminary. Fixed alot of bugs. A
	little bit of cleanup. dbbind() sorta works now, needs work still.
	At least one mem leak that I know of (haven't gotten around to fixing
	it.  Wish I had more time to work on it...
5/2/98  Decided to release what I have. executing sp_who seems to mostly work..
	a step in the right direction.  Version 0.02.  Seem to have generated a
	little interest after mentioning it in a usenet post.
5/6/98	Can compile against sqsh!!! Did a reorg on tds.c, all dblib func that
	read data now go through tds_process_messages() which read the marker
	and calls other routines as necessary. sqsh's output is a little screwy
	(well I don't have a real dbprrow() yet, but the number of result sets
	coming back is too many).  So, anyway Version 0.04
5/9/98  Decided to upload some new code, mostly just stubs.  Sybperl compiles
	I can't get my perl to work with it. (I need to download perl and link
	statically, the one that comes on the system won't do).  Anyway,
	most of dblib is present in stub form.
5/17/98 Managed to scrape up some time and release new code. Duplicate result
	sets went away, and handling of more datatypes (money, bit, more int
	stuff).  Also, improved dbprhead()/row() function.
5/26/98 Ok, I'm doing the long overdue cleaning up of the code. All the kludges
	should be gone.  Thanks, to everyone who contributed
	code/idea/corrections.
6/3/98  The majority of the overhaul is done...still some work to do, but this
        is much better than before. I'm bumping the version to 0.1 signifying
        that I actually use sqsh compiled against it on a regular basis.	
6/5/98	TDS 4.2 support seems to be working properly
6/26/98 Gregg Jenson has added support for err and msg handling among other
	things. I've added some prelimary ctlib support (nothing working yet)
7/3/98	I think we are about ready to release 0.2. Gregg sent some datetime code
	which appears to work great.  I added TDS 4.6 support (small changes
	really) and tested all the byte order issues on an RS/6000.  Also, ctlib
	code will run the unittest.c and will compile all modules in sqsh 1.6,
	however there are many missing functions before it will link!
7/10/98 Haven't been able to work on it lately (moved this week). Anyway,
        trying to add some functions to server.
7/13/98	Tom Poindexter made some changes to get sybtcl to work.
8/8/98	Haven't had much time lately (again), however some small stuff has
	been fixed and the protocol version stuff has (mostly) been moved to a
	runtime option. sybperl is supposedly running for simple stuff.
