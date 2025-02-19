To Do List
==========

This list is ordered top-to-bottom by priority.  
Things that are broken and need mending are at the top, 
followed by things that should work before the next release, 
followed by features that should be added/fixed/reworked (grouped by library).  

Everyone is encouraged to add to the list.  Developers can do it directly; 
anyone else can post a patch to SourceForge.  
In this way we can communicate with each
other about the project's priorities and needs.  

Must be fixed:
====

Work in progress:
====

* be able to disable iconv for BCP (see Sybase documentation)
  I have a patch to disable it, how to handle NVARCHAR? -- freddy77

For future versions (in priority order within library):

All
----

* Cache protocol discovery (TDSVER=0.0). Save port/instance into some permanent storage.
  tsql should report progress in verbose mode.
* retain values used from freetds.conf, so we can report them.
* add a way for tsql to report host, port, and TDS version for 
  the connection it's attempting.
  Actually libTDS does the name resolution in tds_connect and then
  just connect so there is no way for tsql to report these information.
* review the way parameters are packed 
  (too complicate, see ctlib bulk, cf "bulk copy and row buffer")
* improve cursor support on dblib and ctlib
* read on partial packet, do not wait entire one
* support for password longer than 30 characters under Sybase
  (anybody know how ??)
* under Sybase using prepared statements and BLOBs we shouldn't try to
  prepare every time (cache failure preparing, see odbc unittests logs,
  binary_test)
  done in ODBC ??
* Native bcp has no iconv support; character bcp files are assumed be encoded
  with the client's charset.  More flexibility one both sides would be good.  
* encrypted connection for Sybase

db-lib
----

* add DBTEXTLIMIT (dbsetopt), PHP require it to support textlimit ini value

ct-lib
----

* dynamic placeholders (DBD::Sybase)
* ct_option() calls (CS_OPT_ROWCOUNT, CS_OPT_TEXTSIZE, among others)
* async function, async calls (dbpoll() and friends)
* support all type of bind in ct_bind (CS_VARBINARY_TYPE and other)
  search "site:.sybase.com CS_VARBINARY ct_bind" on google for more info
* complete sqlstate and other field in message (for Python)

odbc
----

* Star Office complains that these TypeInfo constants are not implemented in SQLGetInfo:
	47      SQL_USER_NAME
  (handle environment callbacks)
  do a "SELECT USER_NAME()". If data pending MS do another connection with 
  same login.
* SQLNativeSql and fill SQLGetInfo according (mssql7+ handle odbc escapes 
  directly)
* it seems that if statement it's wrong and we issue SQLPrepare on SQLExecute
  it try to send unprepared dynamic... state on dynamic??
* odbc array binding
  test large field (like image) have language queries some limits?
  do we have to split large multiple queries?
* report error just before returning SQL_ERROR from inner function?
* handle async flags ??
* handle no termination on odbc_set_string*

Test and fix
----

* hidden fields (FOR BROWSE select, see flag test on tds)
 * what happen if we bind to an hidden field??
 * if we use SQLGetData??
 * if we request information with SQLDescribeCol/SQLColAttribute(s)/
   SQLGetDescField??
 * as you noted returning # columns hidden fields are not counted (there
   is however a setting which is a mssql extension which threat hidden
   columns as normal)
 (cfr "SQLNumResultCols() is wrong (+1)" Jan 8 2008)
* test: descriptors work
  * what happen to SQL_DESC_DATETIME_INTERVAL_CODE and SQL_DESC_CONCISE_TYPE
    changing only SQL_DESC_TYPE (with SQLSetDescField)
* test: set SQL_C_DEFAULT and call SQLFetch (numeric, others)
* test: SQLGetStmtAttr(SQL_ATTR_ROW_NUMBER)
  * all binded parameters
  * no bind and sqlgetdata
  * before first fetch
  * after last fetch


pool
----

* get connection pooling working with all protocol versions

server
----

* Server API needs more work. It's in a quite ugly state.
  It's more experimental. Should be disabled by default to state it.
