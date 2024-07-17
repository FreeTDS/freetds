#undef NDEBUG
#include "common.h"
#include <assert.h>
#define TDS_DONT_DEFINE_DEFAULT_FUNCTIONS
#include "../../tds/unittests/common.h"
#include <freetds/odbc.h>

/* Check we support any possible types from the server */

static int sql_c_types[100];
static const char *sql_c_types_names[100];
static unsigned num_c_types = 0;

static TDS_STMT *stmt;

static void test_type(TDSSOCKET *tds TDS_UNUSED, TDSCOLUMN *col)
{
	unsigned n;

	/* check that we can get type information from column */
	struct _drecord drec;
	memset(&drec, 0, sizeof(drec));
	odbc_set_sql_type_info(col, &drec, SQL_OV_ODBC3);

	assert(drec.sql_desc_literal_prefix);
	assert(drec.sql_desc_literal_suffix);
	assert(drec.sql_desc_type_name);
	assert(drec.sql_desc_type_name[0]);

	/* check we can attempt to convert from any type to any
	 * SQL C type */
	for (n = 0; n < num_c_types; ++n) {
		TDS_CHAR buffer[256];
		SQLLEN len;
		int sql_c_type = sql_c_types[n];
		const char *sql_c_type_name = sql_c_types_names[n];
		struct _drecord drec_ixd;
		SQLLEN dest_len = sizeof(buffer);
		TDSPARAMINFO *params;

		len = odbc_tds2sql_col(stmt, col, sql_c_type, buffer, sizeof(buffer), NULL);
		if (len == SQL_NULL_DATA) {
			printf("error converting to %3d (%s)\n", sql_c_type, sql_c_type_name);
			continue;
		}

		params = tds_alloc_param_result(NULL);
		assert(params);

		/* convert back to server */
		memset(&drec_ixd, 0, sizeof(drec_ixd));
		drec_ixd.sql_desc_concise_type = sql_c_type;
		drec_ixd.sql_desc_data_ptr = buffer;
		drec_ixd.sql_desc_octet_length_ptr = &dest_len;
		drec_ixd.sql_desc_precision = 18;
		drec_ixd.sql_desc_scale = 4;
		odbc_sql2tds(stmt, &drec_ixd, &drec, params->columns[0], true, stmt->ard, 0);
		tds_free_param_results(params);
	}
}

int
main(void)
{
	TDS_DBC *dbc;
	TDS_ENV *env;
	SQLULEN ulen;
	int i;

	/* extract all C types we support */
	for (i = -200; i <= 200; ++i) {
		assert(num_c_types < 100);
		if (odbc_c_to_server_type(i) != TDS_INVALID_TYPE) {
			sql_c_types[num_c_types] = i;
			sql_c_types_names[num_c_types] = odbc_lookup_value(i, odbc_sql_c_types, NULL);
			assert(sql_c_types_names[num_c_types] != NULL);
			num_c_types++;
		}
	}

	/* this specific test doesn't need a connection, so fake one */
	CHKAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &odbc_env, "S");
	SQLSetEnvAttr(odbc_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);
	CHKAllocHandle(SQL_HANDLE_DBC, odbc_env, &odbc_conn, "S");

	if (!odbc_driver_is_freetds())
		return 0;

	/* get internal structures */
	CHKGetInfo(SQL_DRIVER_HDBC, &ulen, sizeof(ulen), NULL, "S");
	dbc = (TDS_DBC *) (TDS_UINTPTR) ulen;
	CHKGetInfo(SQL_DRIVER_HENV, &ulen, sizeof(ulen), NULL, "S");
	env = (TDS_ENV *) (TDS_UINTPTR) ulen;
	assert(dbc && env);
	assert(env->tds_ctx);

	assert(!dbc->tds_socket);
	dbc->tds_socket = tds_alloc_socket(env->tds_ctx, 512);
	assert(dbc->tds_socket);
	dbc->tds_socket->conn->use_iconv = 0;
	tds_set_parent(dbc->tds_socket, dbc);
	if (TDS_FAILED(tds_iconv_open(dbc->tds_socket->conn, "UTF-8", 1))) {
		fprintf(stderr, "Failed to initialize iconv\n");
		return 1;
	}

	/* get one statement to test with */
	assert(dbc->stmt_list == NULL);
	CHKAllocStmt(&odbc_stmt, "S");
	assert(dbc->stmt_list);
	stmt = dbc->stmt_list;

	tds_all_types(dbc->tds_socket, test_type);

	odbc_disconnect();
	return 0;
}
