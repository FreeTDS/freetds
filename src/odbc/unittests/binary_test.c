/**
 * Summary: Freetds binary patch test.
 * Author:  Gerhard Esterhuizen <ge AT swistgroup.com>
 * Date:    April 2003
 */

#include "common.h"
#include <assert.h>

static char software_version[] = "$Id: binary_test.c,v 1.3 2003-04-30 13:49:07 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define ERR_BUF_SIZE 256
/* 
   Name of table used by the test. Should contain single column,
   of IMAGE type and should contain NO rows at time of program invocation.
*/
#define TEST_TABLE_NAME "#binary_test"

/*
  Size (in bytes) of the test pattern written to and read from
  the database.
*/
#define TEST_BUF_LEN (1024*128)


static SQLRETURN err;
static unsigned char *buf;

static int
sqlreturn_noerr(SQLRETURN rv)
{
	return (rv == SQL_SUCCESS || rv == SQL_SUCCESS_WITH_INFO || rv == SQL_NO_DATA || rv == SQL_NEED_DATA);
}

/* return pointer to ODBC error string: caller owns storage */
static const char *
get_odbc_error(SQLHSTMT stmt_handle)
{
	SQLCHAR *error_buf = (SQLCHAR *) malloc(ERR_BUF_SIZE + 1);
	SQLCHAR sql_state[100];
	SQLINTEGER native_error;
	SQLSMALLINT len;

	err = SQLError(Environment, Connection, stmt_handle, sql_state, &native_error, error_buf, ERR_BUF_SIZE, &len);

	assert(err != SQL_ERROR);

	if (err == SQL_NO_DATA) {
		strncpy((char *) error_buf, "(could not obtain error string)", ERR_BUF_SIZE);
	}
	return (const char *) error_buf;
}

static void
show_error(const char *where, const char *what, int no)
{
	printf("ERROR in %s: %s [%d].\n", where, what, no);
}

static void
clean_up(void)
{
	if (buf)
		free(buf);
	Disconnect();
}

static int
test_insert(void *buf, SQLINTEGER buflen)
{
	SQLHSTMT stmt_handle;
	SQLINTEGER strlen_or_ind;
	const char *qstr = "insert into " TEST_TABLE_NAME " values (?)";

	assert(Connection);
	assert(Environment);

	/* allocate new statement handle */
	err = SQLAllocHandle(SQL_HANDLE_STMT, Connection, &stmt_handle);
	if (!sqlreturn_noerr(err)) {
		show_error("test_insert(): allocating new statement handle", get_odbc_error(0), err);
		return -1;
	}

	/* execute query */
	err = SQLPrepare(stmt_handle, (SQLCHAR *) qstr, SQL_NTS);
	if (!sqlreturn_noerr(err)) {
		show_error("test_insert(): preparing statement", get_odbc_error(stmt_handle), err);
		SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
		stmt_handle = 0;
		return -1;
	}

	strlen_or_ind = buflen;
	err = SQLBindParameter(stmt_handle,
			       1, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARBINARY, (SQLUINTEGER) (-1), 0, buf, buflen,
			       &strlen_or_ind);

	if (!sqlreturn_noerr(err)) {
		show_error("test_insert(): binding to parameter", get_odbc_error(stmt_handle), err);
		SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
		stmt_handle = 0;
		return -1;
	}

	err = SQLExecute(stmt_handle);
	if (!sqlreturn_noerr(err)) {
		show_error("test_insert(): executing prepared query", get_odbc_error(stmt_handle), err);
		SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
		stmt_handle = 0;
		return -1;
	}

	SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
	stmt_handle = 0;
	return 0;
}


static int
test_select(void *buf, SQLINTEGER buflen, SQLINTEGER * bytes_returned)
{
	SQLHSTMT stmt_handle;
	SQLINTEGER strlen_or_ind = 0;
	const char *qstr = "select * from " TEST_TABLE_NAME;

	assert(Connection);
	assert(Environment);

	/* allocate new statement handle */
	err = SQLAllocHandle(SQL_HANDLE_STMT, Connection, &stmt_handle);
	if (!sqlreturn_noerr(err)) {
		show_error("test_select(): allocating new statement handle", get_odbc_error(0), err);
		return -1;
	}

	/* execute query */
	err = SQLExecDirect(stmt_handle, (SQLCHAR *) qstr, SQL_NTS);
	if (!sqlreturn_noerr(err)) {
		show_error("test_select(): executing query", get_odbc_error(stmt_handle), err);
		SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
		stmt_handle = 0;
		return -1;
	}

	/* fetch first page of first result row of unbound column */
	err = SQLFetch(stmt_handle);
	if (!sqlreturn_noerr(err)) {
		show_error("test_select(): fetching results", get_odbc_error(stmt_handle), err);
		SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
		stmt_handle = 0;
		return -1;
	}

	strlen_or_ind = 0;
	err = SQLGetData(stmt_handle, 1, SQL_C_BINARY, buf, buflen, &strlen_or_ind);
	if (!sqlreturn_noerr(err)) {
		show_error("test_select(): getting column", get_odbc_error(stmt_handle), err);
		SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
		stmt_handle = 0;
		return -1;
	}

	*bytes_returned = ((strlen_or_ind > buflen || (strlen_or_ind == SQL_NO_TOTAL)) ? buflen : strlen_or_ind);

	/* ensure that this was the only row */
	err = SQLFetch(stmt_handle);
	if (err != SQL_NO_DATA) {
		show_error("test_select(): retrieving results",
			   "Number of result rows must be exactly equal to 1.\n"
			   "Please delete all entries from table '" TEST_TABLE_NAME "' and rerun test.", err);
		SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
		stmt_handle = 0;
		return -1;
	}

	SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
	stmt_handle = 0;
	return 0;
}

#define BYTE_AT(n) (((n) * 123) & 0xff)

int
main(int argc, char **argv)
{
	int i;
	SQLINTEGER bytes_returned;

	/* do not allocate so big memory in stack */
	buf = (unsigned char *) malloc(TEST_BUF_LEN);

	Connect();

	Command(Statement, "create table " TEST_TABLE_NAME " (im IMAGE)");
	Command(Statement, "SET TEXTSIZE 1000000");

	/* populate test buffer with ramp */
	for (i = 0; i < TEST_BUF_LEN; i++) {
		buf[i] = BYTE_AT(i);
	}

	/* insert test pattern into database */
	if (test_insert(buf, TEST_BUF_LEN) == -1) {
		clean_up();
		return -1;
	}

	memset(buf, 0, TEST_BUF_LEN);

	/* read test pattern from database */
	if (test_select(buf, TEST_BUF_LEN, &bytes_returned) == -1) {
		clean_up();
		return -1;
	}

	/* compare inserted and read back test patterns */
	if (bytes_returned != TEST_BUF_LEN) {
		show_error("main(): comparing buffers", "Mismatch in input and output pattern sizes.", 0);
		clean_up();
		return -1;
	}

	for (i = 0; i < TEST_BUF_LEN; ++i) {
		if (buf[i] != BYTE_AT(i)) {
			printf("mismatch at pos %d %d != %d\n", i, buf[i], BYTE_AT(i));
			show_error("main(): comparing buffers", "Mismatch in input and output patterns.", 0);
			clean_up();
			return -1;
		}
	}

	printf("Input and output buffers of length %d match.\nTest passed.\n", TEST_BUF_LEN);
	clean_up();
	return 0;
}
