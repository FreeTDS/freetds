/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001  Brian Bruns
 * Copyright (C) 2002-2012  Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * \file
 * ODBC catalog functions (SQLTables, SQLColumns and similar) and SQLGetTypeInfo
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <assert.h>

#include <freetds/utils.h>
#include <freetds/odbc.h>
#include <freetds/utils/string.h>
#include <freetds/utils/ascii.h>
#include <odbcss.h>
#include "sqlwparams.h"

static void odbc_upper_column_names(TDS_STMT * stmt);
static void odbc_col_setname(TDS_STMT * stmt, int colpos, const char *name);
static SQLRETURN odbc_stat_execute(TDS_STMT * stmt _WIDE, const char *begin, int nparams, ...);

static void
odbc_col_setname(TDS_STMT *stmt, int colpos, const char *name)
{
#if ENABLE_EXTRA_CHECKS
	odbc_ird_check(stmt);

	if (colpos > 0 && stmt->tds != NULL) {
		TDSRESULTINFO *resinfo = stmt->tds->current_results;

		if (resinfo != NULL && colpos <= resinfo->num_cols) {
			/* no overflow possible, name is always shorter */
			if (!tds_dstr_copy(&resinfo->columns[colpos - 1]->column_name, name))
				odbc_errs_add(&stmt->errs, "HY001", NULL);
			tds_dstr_empty(&resinfo->columns[colpos - 1]->table_column_name);
		}
	}
#endif

	if (colpos > 0 && colpos <= stmt->ird->header.sql_desc_count) {
		--colpos;
		if (!tds_dstr_copy(&stmt->ird->records[colpos].sql_desc_label, name)
		    || !tds_dstr_copy(&stmt->ird->records[colpos].sql_desc_name, name))
			odbc_errs_add(&stmt->errs, "HY001", NULL);
	}
}

ODBC_FUNC(SQLColumnPrivileges, (P(SQLHSTMT, hstmt), PCHARIN(CatalogName, SQLSMALLINT), PCHARIN(SchemaName, SQLSMALLINT),
				PCHARIN(TableName, SQLSMALLINT), PCHARIN(ColumnName, SQLSMALLINT) WIDE))
{
	int retcode;

	ODBC_ENTER_HSTMT;

	retcode =
		odbc_stat_execute(stmt _wide, "sp_column_privileges", 4, "O@table_qualifier", szCatalogName, cbCatalogName,
				  "O@table_owner", szSchemaName, cbSchemaName, "O@table_name", szTableName, cbTableName,
				  "P@column_name", szColumnName, cbColumnName);
	if (SQL_SUCCEEDED(retcode) && stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
	}
	ODBC_EXIT_(stmt);
}

ODBC_FUNC(SQLForeignKeys, (P(SQLHSTMT, hstmt), PCHARIN(PkCatalogName, SQLSMALLINT),
			   PCHARIN(PkSchemaName, SQLSMALLINT), PCHARIN(PkTableName, SQLSMALLINT),
			   PCHARIN(FkCatalogName, SQLSMALLINT), PCHARIN(FkSchemaName, SQLSMALLINT),
			   PCHARIN(FkTableName, SQLSMALLINT) WIDE))
{
	int retcode;

	ODBC_ENTER_HSTMT;

	retcode =
		odbc_stat_execute(stmt _wide, "sp_fkeys", 6,
				  "O@pktable_qualifier", szPkCatalogName, cbPkCatalogName,
				  "O@pktable_owner", szPkSchemaName, cbPkSchemaName,
				  "O@pktable_name", szPkTableName, cbPkTableName,
				  "O@fktable_qualifier", szFkCatalogName, cbFkCatalogName,
				  "O@fktable_owner", szFkSchemaName, cbFkSchemaName,
				  "O@fktable_name", szFkTableName, cbFkTableName);
	if (SQL_SUCCEEDED(retcode) && stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) {
		odbc_col_setname(stmt, 1, "PKTABLE_CAT");
		odbc_col_setname(stmt, 2, "PKTABLE_SCHEM");
		odbc_col_setname(stmt, 5, "FKTABLE_CAT");
		odbc_col_setname(stmt, 6, "FKTABLE_SCHEM");
	}
	ODBC_EXIT_(stmt);
}

ODBC_FUNC(SQLPrimaryKeys, (P(SQLHSTMT, hstmt), PCHARIN(CatalogName, SQLSMALLINT),
			   PCHARIN(SchemaName, SQLSMALLINT), PCHARIN(TableName, SQLSMALLINT) WIDE))
{
	int retcode;

	ODBC_ENTER_HSTMT;

	retcode =
		odbc_stat_execute(stmt _wide, "sp_pkeys", 3, "O@table_qualifier", szCatalogName, cbCatalogName, "O@table_owner",
				  szSchemaName, cbSchemaName, "O@table_name", szTableName, cbTableName);
	if (SQL_SUCCEEDED(retcode) && stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
	}
	ODBC_EXIT_(stmt);
}

ODBC_FUNC(SQLProcedureColumns, (P(SQLHSTMT, hstmt), PCHARIN(CatalogName, SQLSMALLINT),
				PCHARIN(SchemaName, SQLSMALLINT), PCHARIN(ProcName, SQLSMALLINT),
				PCHARIN(ColumnName, SQLSMALLINT) WIDE))
{
	int retcode;

	ODBC_ENTER_HSTMT;

	retcode =
		odbc_stat_execute(stmt _wide, "sp_sproc_columns", TDS_IS_MSSQL(stmt->dbc->tds_socket) ? 5 : 4,
				  "O@procedure_qualifier", szCatalogName, cbCatalogName,
				  "P@procedure_owner", szSchemaName, cbSchemaName, "P@procedure_name", szProcName, cbProcName,
				  "P@column_name", szColumnName, cbColumnName, "V@ODBCVer", (char *) NULL, 0);
	if (SQL_SUCCEEDED(retcode) && stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) {
		odbc_col_setname(stmt, 1, "PROCEDURE_CAT");
		odbc_col_setname(stmt, 2, "PROCEDURE_SCHEM");
		odbc_col_setname(stmt, 8, "COLUMN_SIZE");
		odbc_col_setname(stmt, 9, "BUFFER_LENGTH");
		odbc_col_setname(stmt, 10, "DECIMAL_DIGITS");
		odbc_col_setname(stmt, 11, "NUM_PREC_RADIX");
		if (TDS_IS_SYBASE(stmt->dbc->tds_socket))
			stmt->special_row = ODBC_SPECIAL_PROCEDURECOLUMNS;
	}
	ODBC_EXIT_(stmt);
}

ODBC_FUNC(SQLProcedures, (P(SQLHSTMT, hstmt), PCHARIN(CatalogName, SQLSMALLINT),
			  PCHARIN(SchemaName, SQLSMALLINT), PCHARIN(ProcName, SQLSMALLINT) WIDE))
{
	int retcode;

	ODBC_ENTER_HSTMT;

	retcode =
		odbc_stat_execute(stmt _wide, "..sp_stored_procedures", 3,
				  "P@sp_name", szProcName, cbProcName,
				  "P@sp_owner", szSchemaName, cbSchemaName, "O@sp_qualifier", szCatalogName, cbCatalogName);
	if (SQL_SUCCEEDED(retcode) && stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) {
		odbc_col_setname(stmt, 1, "PROCEDURE_CAT");
		odbc_col_setname(stmt, 2, "PROCEDURE_SCHEM");
	}
	ODBC_EXIT_(stmt);
}

ODBC_FUNC(SQLTablePrivileges, (P(SQLHSTMT, hstmt), PCHARIN(CatalogName, SQLSMALLINT),
			       PCHARIN(SchemaName, SQLSMALLINT), PCHARIN(TableName, SQLSMALLINT) WIDE))
{
	int retcode;

	ODBC_ENTER_HSTMT;

	retcode =
		odbc_stat_execute(stmt _wide, "sp_table_privileges", 3, "O@table_qualifier", szCatalogName, cbCatalogName,
				  "P@table_owner", szSchemaName, cbSchemaName, "P@table_name", szTableName, cbTableName);
	if (SQL_SUCCEEDED(retcode) && stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
	}
	ODBC_EXIT_(stmt);
}

/**
 * SQLColumns
 *
 * Return column information for a table or view. This is
 * mapped to a call to sp_columns which - lucky for us - returns
 * the exact result set we need.
 *
 * exec sp_columns [ @table_name = ] object
 *                 [ , [ @table_owner = ] owner ]
 *                 [ , [ @table_qualifier = ] qualifier ]
 *                 [ , [ @column_name = ] column ]
 *                 [ , [ @ODBCVer = ] ODBCVer ]
 *
 */
ODBC_FUNC(SQLColumns, (P(SQLHSTMT, hstmt), PCHARIN(CatalogName, SQLSMALLINT),	/* object_qualifier */
		       PCHARIN(SchemaName, SQLSMALLINT),	/* object_owner */
		       PCHARIN(TableName, SQLSMALLINT),	/* object_name */
		       PCHARIN(ColumnName, SQLSMALLINT)	/* column_name */
		       WIDE))
{
	int retcode;
	const char *proc = "sp_columns";

	ODBC_ENTER_HSTMT;

	if (odbc_get_string_size(cbCatalogName, szCatalogName _wide))
		proc = "..sp_columns";

	retcode =
		odbc_stat_execute(stmt _wide, proc, TDS_IS_MSSQL(stmt->dbc->tds_socket) ? 5 : 4,
				  "P@table_name", szTableName, cbTableName, "P@table_owner", szSchemaName,
				  cbSchemaName, "O@table_qualifier", szCatalogName, cbCatalogName, "P@column_name", szColumnName,
				  cbColumnName, "V@ODBCVer", (char *) NULL, 0);
	if (SQL_SUCCEEDED(retcode) && stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
		odbc_col_setname(stmt, 7, "COLUMN_SIZE");
		odbc_col_setname(stmt, 8, "BUFFER_LENGTH");
		odbc_col_setname(stmt, 9, "DECIMAL_DIGITS");
		odbc_col_setname(stmt, 10, "NUM_PREC_RADIX");
		if (TDS_IS_SYBASE(stmt->dbc->tds_socket))
			stmt->special_row = ODBC_SPECIAL_COLUMNS;
	}
	ODBC_EXIT_(stmt);
}

static void
odbc_upper_column_names(TDS_STMT *stmt)
{
	int icol;
	TDS_DESC *ird;

#if ENABLE_EXTRA_CHECKS
	TDSSOCKET *tds;

	odbc_ird_check(stmt);

	tds = stmt->dbc->tds_socket;
	if (tds && tds->current_results) {
		TDSRESULTINFO *const resinfo = tds->current_results;

		for (icol = 0; icol < resinfo->num_cols; ++icol) {
			TDSCOLUMN *colinfo = resinfo->columns[icol];

			/* upper case */
			tds_ascii_strupr(tds_dstr_buf(&colinfo->column_name));
		}
	}
#endif

	ird = stmt->ird;
	for (icol = ird->header.sql_desc_count; --icol >= 0;) {
		struct _drecord *drec = &ird->records[icol];

		/* upper case */
		tds_ascii_strupr(tds_dstr_buf(&drec->sql_desc_label));
		tds_ascii_strupr(tds_dstr_buf(&drec->sql_desc_name));
	}
}

SQLRETURN ODBC_PUBLIC ODBC_API
SQLGetTypeInfo(SQLHSTMT hstmt, SQLSMALLINT fSqlType)
{
	SQLRETURN res;
	TDSSOCKET *tds;
	TDS_INT result_type;
	TDS_INT compute_id;
	int varchar_pos = -1, n;
	static const char sql_templ_default[] = "sp_datatype_info %d";
	const char *sql_templ = sql_templ_default;
	char sql[sizeof(sql_templ_default) + 36];
	int odbc3;

	ODBC_ENTER_HSTMT;

	tdsdump_log(TDS_DBG_FUNC, "SQLGetTypeInfo(%p, %d)\n", hstmt, fSqlType);

	tds = stmt->dbc->tds_socket;
	odbc3 = (stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3);

	if (IS_TDS73_PLUS(tds->conn))
		sql_templ = "sp_datatype_info_100 %d";
	else if (IS_TDS72_PLUS(tds->conn))
		sql_templ = "sp_datatype_info_90 %d";

	/* For MSSQL6.5 and Sybase 11.9 sp_datatype_info work */
	/* TODO what about early Sybase products ? */
	/* TODO Does Sybase return all ODBC3 columns? Add them if not */
	/* TODO ODBC3 convert type to ODBC version 2 (date) */
	if (odbc3) {
		if (TDS_IS_SYBASE(tds)) {
			sprintf(sql, sql_templ, odbc_swap_datetime_sql_type(fSqlType, 0));
			stmt->special_row = ODBC_SPECIAL_GETTYPEINFO;
		} else {
			sprintf(sql, sql_templ, fSqlType);
			strcat(sql, ",3");
		}
	} else {
		/* MS ODBC translate SQL_TIMESTAMP to SQL_TYPE_TIMESTAMP even for ODBC 2 apps
		 * other DM just pass value straight.
		 * Convert always to version 2 to make version 2 datatype works with all DMs
		 */
		sprintf(sql, sql_templ, odbc_swap_datetime_sql_type(fSqlType, 2));
	}
	if (SQL_SUCCESS != odbc_set_stmt_query(stmt, (ODBC_CHAR *) sql, strlen(sql) _wide0))
		ODBC_EXIT(stmt, SQL_ERROR);

redo:
	res = odbc_SQLExecute(stmt);

	odbc_upper_column_names(stmt);
	if (odbc3) {
		odbc_col_setname(stmt, 3, "COLUMN_SIZE");
		odbc_col_setname(stmt, 11, "FIXED_PREC_SCALE");
		odbc_col_setname(stmt, 12, "AUTO_UNIQUE_VALUE");
	}

	/* workaround for a mispelled column name in Sybase */
	if (TDS_IS_SYBASE(stmt->dbc->tds_socket) && !odbc3)
		odbc_col_setname(stmt, 3, "PRECISION");

	if (TDS_IS_MSSQL(stmt->dbc->tds_socket) || fSqlType != SQL_VARCHAR || res != SQL_SUCCESS)
		ODBC_EXIT(stmt, res);

	/*
	 * Sybase return first nvarchar for char... and without length !!!
	 * Some program use first entry so we discard all entry before varchar
	 */
	n = 0;
	while (tds->current_results) {
		TDSRESULTINFO *resinfo;
		TDSCOLUMN *colinfo;
		char *name;

		/* if next is varchar leave next for SQLFetch */
		if (n == (varchar_pos - 1))
			break;

		switch (tds_process_tokens(stmt->dbc->tds_socket, &result_type, &compute_id, TDS_STOPAT_ROWFMT | TDS_RETURN_ROW)) {
		case TDS_SUCCESS:
			if (result_type == TDS_ROW_RESULT)
				break;
		case TDS_NO_MORE_RESULTS:
			/* discard other tokens */
			tds_process_simple_query(tds);
			if (n >= varchar_pos && varchar_pos > 0)
				goto redo;
			break;
		case TDS_CANCELLED:
			odbc_errs_add(&stmt->errs, "HY008", NULL);
			res = SQL_ERROR;
			break;
		}
		if (!tds->current_results)
			break;
		++n;

		resinfo = tds->current_results;
		colinfo = resinfo->columns[0];
		name = (char *) colinfo->column_data;
		if (is_blob_col(colinfo))
			name = (char *) ((TDSBLOB *) name)->textvalue;
		/* skip nvarchar and sysname */
		if (colinfo->column_cur_size == 7 && memcmp("varchar", name, 7) == 0) {
			varchar_pos = n;
		}
	}
	ODBC_EXIT(stmt, res);
}

#ifdef ENABLE_ODBC_WIDE
SQLRETURN ODBC_PUBLIC ODBC_API
SQLGetTypeInfoW(SQLHSTMT hstmt, SQLSMALLINT fSqlType)
{
	return SQLGetTypeInfo(hstmt, fSqlType);
}
#endif

ODBC_FUNC(SQLSpecialColumns, (P(SQLHSTMT, hstmt), P(SQLUSMALLINT, fColType), PCHARIN(CatalogName, SQLSMALLINT),
			      PCHARIN(SchemaName, SQLSMALLINT), PCHARIN(TableName, SQLSMALLINT),
			      P(SQLUSMALLINT, fScope), P(SQLUSMALLINT, fNullable) WIDE))
{
	int retcode;
	char nullable, scope, col_type;

	ODBC_ENTER_HSTMT;

	tdsdump_log(TDS_DBG_FUNC, "SQLSpecialColumns(%p, %d, %p, %d, %p, %d, %p, %d, %d, %d)\n",
		    hstmt, fColType, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName,
		    fScope, fNullable);

#ifdef TDS_NO_DM
	/* Check column type */
	if (fColType != SQL_BEST_ROWID && fColType != SQL_ROWVER) {
		odbc_errs_add(&stmt->errs, "HY097", NULL);
		ODBC_EXIT_(stmt);
	}

	/* check our buffer lengths */
	if (!IS_VALID_LEN(cbCatalogName) || !IS_VALID_LEN(cbSchemaName) || !IS_VALID_LEN(cbTableName)) {
		odbc_errs_add(&stmt->errs, "HY090", NULL);
		ODBC_EXIT_(stmt);
	}

	/* Check nullable */
	if (fNullable != SQL_NO_NULLS && fNullable != SQL_NULLABLE) {
		odbc_errs_add(&stmt->errs, "HY099", NULL);
		ODBC_EXIT_(stmt);
	}

	if (!odbc_get_string_size(cbTableName, szTableName _wide)) {
		odbc_errs_add(&stmt->errs, "HY009", "SQLSpecialColumns: The table name parameter is required");
		ODBC_EXIT_(stmt);
	}

	switch (fScope) {
	case SQL_SCOPE_CURROW:
	case SQL_SCOPE_TRANSACTION:
	case SQL_SCOPE_SESSION:
		break;
	default:
		odbc_errs_add(&stmt->errs, "HY098", NULL);
		ODBC_EXIT_(stmt);
	}
#endif

	if (fNullable == SQL_NO_NULLS)
		nullable = 'O';
	else
		nullable = 'U';

	if (fScope == SQL_SCOPE_CURROW)
		scope = 'C';
	else
		scope = 'T';

	if (fColType == SQL_BEST_ROWID)
		col_type = 'R';
	else
		col_type = 'V';

	retcode =
		odbc_stat_execute(stmt _wide, "sp_special_columns", TDS_IS_MSSQL(stmt->dbc->tds_socket) ? 7 : 4, "O", szTableName,
				  cbTableName, "O", szSchemaName, cbSchemaName, "O@qualifier", szCatalogName, cbCatalogName,
				  "!@col_type", &col_type, 1, "!@scope", &scope, 1, "!@nullable", &nullable, 1,
				  "V@ODBCVer", (char *) NULL, 0);
	if (SQL_SUCCEEDED(retcode) && stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) {
		odbc_col_setname(stmt, 5, "COLUMN_SIZE");
		odbc_col_setname(stmt, 6, "BUFFER_LENGTH");
		odbc_col_setname(stmt, 7, "DECIMAL_DIGITS");
		if (TDS_IS_SYBASE(stmt->dbc->tds_socket))
			stmt->special_row = ODBC_SPECIAL_SPECIALCOLUMNS;
	}
	ODBC_EXIT_(stmt);
}

ODBC_FUNC(SQLStatistics, (P(SQLHSTMT, hstmt), PCHARIN(CatalogName, SQLSMALLINT),
			  PCHARIN(SchemaName, SQLSMALLINT), PCHARIN(TableName, SQLSMALLINT), P(SQLUSMALLINT, fUnique),
			  P(SQLUSMALLINT, fAccuracy) WIDE))
{
	int retcode;
	char unique, accuracy;

	ODBC_ENTER_HSTMT;

	tdsdump_log(TDS_DBG_FUNC, "SQLStatistics(%p, %p, %d, %p, %d, %p, %d, %d, %d)\n",
		    hstmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName, fUnique, fAccuracy);

#ifdef TDS_NO_DM
	/* check our buffer lengths */
	if (!IS_VALID_LEN(cbCatalogName) || !IS_VALID_LEN(cbSchemaName) || !IS_VALID_LEN(cbTableName)) {
		odbc_errs_add(&stmt->errs, "HY090", NULL);
		ODBC_EXIT_(stmt);
	}

	/* check our uniqueness value */
	if (fUnique != SQL_INDEX_UNIQUE && fUnique != SQL_INDEX_ALL) {
		odbc_errs_add(&stmt->errs, "HY100", NULL);
		ODBC_EXIT_(stmt);
	}

	/* check our accuracy value */
	if (fAccuracy != SQL_QUICK && fAccuracy != SQL_ENSURE) {
		odbc_errs_add(&stmt->errs, "HY101", NULL);
		ODBC_EXIT_(stmt);
	}

	if (!odbc_get_string_size(cbTableName, szTableName _wide)) {
		odbc_errs_add(&stmt->errs, "HY009", NULL);
		ODBC_EXIT_(stmt);
	}
#endif

	if (fAccuracy == SQL_ENSURE)
		accuracy = 'E';
	else
		accuracy = 'Q';

	if (fUnique == SQL_INDEX_UNIQUE)
		unique = 'Y';
	else
		unique = 'N';

	retcode =
		odbc_stat_execute(stmt _wide, "sp_statistics", TDS_IS_MSSQL(stmt->dbc->tds_socket) ? 5 : 4, "O@table_qualifier",
				  szCatalogName, cbCatalogName, "O@table_owner", szSchemaName, cbSchemaName, "O@table_name",
				  szTableName, cbTableName, "!@is_unique", &unique, 1, "!@accuracy", &accuracy, 1);
	if (SQL_SUCCEEDED(retcode) && stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
		odbc_col_setname(stmt, 8, "ORDINAL_POSITION");
		odbc_col_setname(stmt, 10, "ASC_OR_DESC");
	}
	ODBC_EXIT_(stmt);
}

ODBC_FUNC(SQLTables, (P(SQLHSTMT, hstmt), PCHARIN(CatalogName, SQLSMALLINT),
		      PCHARIN(SchemaName, SQLSMALLINT), PCHARIN(TableName, SQLSMALLINT), PCHARIN(TableType, SQLSMALLINT) WIDE))
{
	int retcode;
	const char *proc = NULL;
	int wildcards;
	TDSSOCKET *tds;
	DSTR schema_name = DSTR_INITIALIZER;
	DSTR catalog_name = DSTR_INITIALIZER;
	DSTR table_name = DSTR_INITIALIZER;
	DSTR table_type = DSTR_INITIALIZER;

	ODBC_ENTER_HSTMT;

	tdsdump_log(TDS_DBG_FUNC, "SQLTables(%p, %p, %d, %p, %d, %p, %d, %p, %d)\n",
		    hstmt, szCatalogName, cbCatalogName, szSchemaName, cbSchemaName, szTableName, cbTableName,
		    szTableType, cbTableType);

	tds = stmt->dbc->tds_socket;

	if (!odbc_dstr_copy(stmt->dbc, &catalog_name, cbCatalogName, szCatalogName)
	    || !odbc_dstr_copy(stmt->dbc, &schema_name, cbSchemaName, szSchemaName)
	    || !odbc_dstr_copy(stmt->dbc, &table_name, cbTableName, szTableName)
	    || !odbc_dstr_copy(stmt->dbc, &table_type, cbTableType, szTableType))
		goto memory_error;

	if (cbTableName == SQL_NTS)
		cbTableName = tds_dstr_len(&table_name);
	if (cbSchemaName == SQL_NTS)
		cbSchemaName = tds_dstr_len(&schema_name);

	/* support wildcards on catalog (only odbc 3) */
	wildcards = 0;
	if (stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3 && stmt->dbc->attr.metadata_id == SQL_FALSE &&
	    (strchr(tds_dstr_cstr(&catalog_name), '%') || strchr(tds_dstr_cstr(&catalog_name), '_')))
		wildcards = 1;

	proc = "sp_tables";
	if (!tds_dstr_isempty(&catalog_name)) {
		if (wildcards) {
			/* if catalog specified and wildcards use sp_tableswc under mssql2k */
			if (TDS_IS_MSSQL(tds) && tds->conn->product_version >= TDS_MS_VER(8, 0, 0)) {
				proc = "sp_tableswc";
				if (tds_dstr_isempty(&schema_name))
					if (!tds_dstr_copy(&schema_name, "%"))
						goto memory_error;
			}
			/*
			 * TODO support wildcards on catalog even for Sybase
			 * first execute a select name from master..sysdatabases where name like catalog quoted
			 * build a db1..sp_tables args db2..sp_tables args ... query
			 * collapse results in a single recordset (how??)
			 */
		} else {
			/* if catalog specified and not wildcards use catatog on name (catalog..sp_tables) */
			proc = "..sp_tables";
		}
	}

	/* fix type if needed quoting it */
	if (!tds_dstr_isempty(&table_type)) {
		int to_fix = 0;
		int elements = 0;
		const char *p = tds_dstr_cstr(&table_type);
		const char *const end = p + tds_dstr_len(&table_type);

		for (;;) {
			const char *begin = p;

			p = (const char *) memchr(p, ',', end - p);
			if (!p)
				p = end;
			++elements;
			if ((p - begin) < 2 || begin[0] != '\'' || p[-1] != '\'')
				to_fix = 1;
			if (p >= end)
				break;
			++p;
		}
		/* fix it */
		tdsdump_log(TDS_DBG_INFO1, "to_fix %d elements %d\n", to_fix, elements);
		if (to_fix) {
			char *dst, *type;

			tdsdump_log(TDS_DBG_INFO1, "fixing type elements\n");
			type = tds_new(char, tds_dstr_len(&table_type) + elements * 2 + 3);

			if (!type)
				goto memory_error;

			p = tds_dstr_cstr(&table_type);
			dst = type;
			for (;;) {
				const char *begin = p;

				p = (const char *) memchr(p, ',', end - p);
				if (!p)
					p = end;
				if ((p - begin) < 2 || begin[0] != '\'' || p[-1] != '\'') {
					*dst++ = '\'';
					memcpy(dst, begin, p - begin);
					dst += p - begin;
					*dst++ = '\'';
				} else {
					memcpy(dst, begin, p - begin);
					dst += p - begin;
				}
				if (p >= end)
					break;
				*dst++ = *p++;
			}
			*dst = 0;
			if (!tds_dstr_set(&table_type, type)) {
				free(type);
				goto memory_error;
			}
		}
	}

	/* special case for catalog list */
	if (strcmp(tds_dstr_cstr(&catalog_name), "%") == 0 && cbTableName <= 0 && cbSchemaName <= 0) {
		retcode =
			odbc_stat_execute(stmt _wide, "sp_tables", 3, "$!P@table_name", "", 0,
					  "$!P@table_owner", "", 0, "!P@table_qualifier", "%", 1);
	} else {
		retcode =
			odbc_stat_execute(stmt _wide, proc, 4,
					  "!P@table_name", tds_dstr_cstr(&table_name), tds_dstr_len(&table_name),
					  "!P@table_owner", tds_dstr_cstr(&schema_name), tds_dstr_len(&schema_name),
					  "!P@table_qualifier", tds_dstr_cstr(&catalog_name), tds_dstr_len(&catalog_name),
					  "!@table_type", tds_dstr_cstr(&table_type), tds_dstr_len(&table_type));
	}
	tds_dstr_free(&schema_name);
	tds_dstr_free(&catalog_name);
	tds_dstr_free(&table_name);
	tds_dstr_free(&table_type);
	if (SQL_SUCCEEDED(retcode) && stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) {
		odbc_col_setname(stmt, 1, "TABLE_CAT");
		odbc_col_setname(stmt, 2, "TABLE_SCHEM");
	}
	ODBC_EXIT_(stmt);

memory_error:
	tds_dstr_free(&schema_name);
	tds_dstr_free(&catalog_name);
	tds_dstr_free(&table_name);
	tds_dstr_free(&table_type);
	odbc_errs_add(&stmt->errs, "HY001", NULL);
	ODBC_EXIT_(stmt);
}

static size_t
odbc_quote_metadata(TDS_DBC *dbc, char type, char *dest, DSTR *dstr)
{
	int unquote = 0;
	char prev, buf[1200], *dst;
	const char *s = tds_dstr_cstr(dstr);
	ptrdiff_t len = tds_dstr_len(dstr);

	/* limit string/id lengths */
	if (len > 384)
		len = 384;

	if (!type || (type == 'O' && dbc->attr.metadata_id == SQL_FALSE)) {
		if (dest)
			memcpy(dest, s, len);
		return len;
	}

	/* where we can have ID or PV */
	assert(type == 'P' || (type == 'O' && dbc->attr.metadata_id != SQL_FALSE));

	/* ID ? */
	if (dbc->attr.metadata_id != SQL_FALSE) {
		/* strip leading and trailing spaces */
		while (len > 0 && *s == ' ')
			++s, --len;
		while (len > 0 && s[len - 1] == ' ')
			--len;
		/* unquote if necessary */
		if (len > 2 && *s == '\"' && s[len - 1] == '\"') {
			++s, len -= 2;
			unquote = 1;
		}
	}

	if (!dest)
		dest = buf;
	dst = dest;

	/*
	 * handle patterns
	 * "'" -> "''" (normal string quoting)
	 *
	 * if metadata_id is FALSE
	 * "\_" -> "[_]"
	 * "\%" -> "[%]"
	 * "[" -> "[[]"
	 *
	 * if metadata_id is TRUE
	 * "\"\"" -> "\"" (if unquote id)
	 * "_" -> "[_]"
	 * "%" -> "[%]"
	 * "[" -> "[[]"
	 */
	prev = 0;
	for (; --len >= 0; ++s) {
		switch (*s) {
		case '\"':
			if (unquote && prev == '\"') {
				prev = 0;	/* avoid "\"\"\"" -> "\"" */
				--dst;
				continue;
			}
			break;
		case '_':
		case '%':
			if (dbc->attr.metadata_id == SQL_FALSE) {
				if (prev != '\\')
					break;
				--dst;
			}
		case '[':
			if (type != 'P')
				break;
			/* quote search string */
			*dst++ = '[';
			*dst++ = *s;
			*dst++ = ']';
			prev = 0;
			continue;
		}
		*dst++ = prev = *s;
	}
	return dst - dest;
}

static SQLRETURN
odbc_stat_execute(TDS_STMT *stmt _WIDE, const char *begin, int nparams, ...)
{
	int i, param_qualifier = -1;
	size_t len;
	char *proc = NULL, *p;
	SQLRETURN retcode;
	va_list marker;
	DSTR qualifier = DSTR_INITIALIZER, value = DSTR_INITIALIZER;
	TDSPARAMINFO *params;

	/* read all params and calc len required */
	va_start(marker, nparams);
	len = strlen(begin) + 3;
	params = tds_alloc_results(0);
	if (!params)
		goto mem_error;
	for (i = 0; i < nparams; ++i) {
		int param_len;
		bool convert TDS_UNUSED = true;
		bool add_always = false;
		DSTR *out;
		const char *name;
		char type;

		p = va_arg(marker, char *);

		for (;; ++p) {
			switch (*p) {
			case '$':
				add_always = true;
				continue;
			case '!':
				convert = false;
				continue;
			case 'V':	/* ODBC version */
			case 'O':	/* ordinary arguments */
			case 'P':	/* pattern value arguments */
				type = *p++;
				break;
			default:
				type = 0;	/* ordinary type */
				break;
			}
			break;
		}
		name = p;

		p = va_arg(marker, char *);
		param_len = va_arg(marker, int);

		if (type == 'V') {
			int ver = (stmt->dbc->env->attr.odbc_version == SQL_OV_ODBC3) ? 3 : 2;

			if (!odbc_add_int_param(stmt->dbc->tds_socket, params, name, ver))
				goto mem_error;
			continue;
		}

#ifdef ENABLE_ODBC_WIDE
		if (!convert)
			out = tds_dstr_copyn(&value, p, param_len);
		else
			out = odbc_dstr_copy(stmt->dbc, &value, param_len, (ODBC_CHAR *) p);
#else
		out = odbc_dstr_copy(stmt->dbc, &value, param_len, (ODBC_CHAR *) p);
#endif
		if (!out)
			goto mem_error;

		if (add_always || !tds_dstr_isempty(&value)) {
			char buf[1200];
			int l;

			l = (int) odbc_quote_metadata(stmt->dbc, type, buf, &value);
			if (!odbc_add_char_param(stmt->dbc->tds_socket, params, name, buf, l))
				goto mem_error;

			if (begin[0] == '.' && strstr(name, "qualifier")) {
				if (!tds_dstr_dup(&qualifier, &value))
					goto mem_error;
				len += tds_quote_id_rpc(stmt->dbc->tds_socket, NULL,
							tds_dstr_cstr(&qualifier), tds_dstr_len(&qualifier));
				param_qualifier = i;
			}
		}
	}
	tds_dstr_free(&value);

	/* proc is neither mb or wide, is always utf encoded */
	retcode = odbc_set_stmt_query(stmt, (ODBC_CHAR *) "-", 1 _wide0);
	if (retcode != SQL_SUCCESS)
		goto error;
	stmt->prepared_query_is_rpc = 1;

	/* set params */
	tds_free_param_results(stmt->params);
	stmt->param_count = params->num_cols;
	stmt->param_num = 1 + stmt->param_count;
	stmt->params = params;
	params = NULL;

	/* allocate space for string */
	if (!tds_dstr_alloc(&stmt->query, len))
		goto mem_error;
	proc = tds_dstr_buf(&stmt->query);

	/* build query string */
	p = proc;
	if (param_qualifier >= 0)
		p += tds_quote_id_rpc(stmt->dbc->tds_socket, p, tds_dstr_cstr(&qualifier), tds_dstr_len(&qualifier));
	tds_dstr_free(&qualifier);
	strcpy(p, begin);
	p += strlen(begin);
	tds_dstr_setlen(&stmt->query, p - proc);
	assert(p + 1 <= proc + len);

	/* execute it */
	retcode = odbc_SQLExecute(stmt);
	if (SQL_SUCCEEDED(retcode))
		odbc_upper_column_names(stmt);

	va_end(marker);
	ODBC_RETURN(stmt, retcode);

mem_error:
	odbc_errs_add(&stmt->errs, "HY001", NULL);

error:
	tds_dstr_free(&value);
	tds_dstr_free(&qualifier);
	tds_free_results(params);
	va_end(marker);
	return SQL_ERROR;
}

#include "catalog_export.h"
