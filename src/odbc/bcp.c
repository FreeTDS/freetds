/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2010, 2011  Frediano Ziglio
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

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef _WIN32
#include <io.h>
#endif

#include <freetds/tds.h>
#include <freetds/iconv.h>
#include <freetds/convert.h>
#include <freetds/odbc.h>
#include <freetds/string.h>
#define TDSODBC_BCP
#include <odbcss.h>

static TDSRET
_bcp_get_col_data(TDSBCPINFO *bcpinfo, TDSCOLUMN *bindcol, int offset);
static SQLLEN
_bcp_get_term_var(const TDS_CHAR * pdata, const TDS_CHAR * term, int term_len);

#define ODBCBCP_ERROR_RETURN(code) \
	do {odbc_errs_add(&dbc->errs, code, NULL); return;} while(0)

#define ODBCBCP_ERROR_DBINT(code) \
	do {odbc_errs_add(&dbc->errs, code, NULL); return -1;} while(0)

/**
 * \ingroup odbc_bcp
 * \brief Prepare for bulk copy operation on a table
 *
 * \param dbc ODBC database connection object
 * \param tblname the name of the table receiving or providing the data.
 * \param hfile the data file opposite the table, if any. NB: The current
 *              implementation does not support file I/O so this must be NULL
 * \param errfile the "error file" captures messages and, if errors are
 *              encountered. NB: The current implementation does not support
 *              file I/O so this must be NULL
 * \param direction one of
 *      - \b DB_IN writing to the table
 *      - \b DB_OUT writing to the host file (Not currently supported)
 *      .
 * \remarks bcp_init() sets the host file data format and acquires the table metadata.
 *	It is called before the other bulk copy functions.
 *
 *	The ODBC BCP functionality should be accessed via the inline functions in
 *	odbcss.h.
 *
 *	After calling this function, call bcp_bind() to associate your data with
 *	the appropriate table column.
 *
 * \sa	SQL_COPT_SS_BCP, odbc_bcp_bind(), odbc_bcp_done(), odbc_bcp_exec()
 */
void
odbc_bcp_init(TDS_DBC *dbc, const ODBC_CHAR *tblname, const ODBC_CHAR *hfile,
	      const ODBC_CHAR *errfile, int direction _WIDE)
{
	if (TDS_UNLIKELY(tds_write_dump)) {
#ifdef ENABLE_ODBC_WIDE
		if (wide) {
			SQLWSTR_BUFS(3);
			tdsdump_log(TDS_DBG_FUNC, "bcp_initW(%p, %ls, %ls, %ls, %d)\n",
				    dbc, SQLWSTR(tblname->wide), SQLWSTR(hfile->wide), SQLWSTR(errfile->wide), direction);
			SQLWSTR_FREE();
		} else {
#endif
		tdsdump_log(TDS_DBG_FUNC, "bcp_init(%p, %s, %s, %s, %d)\n",
			    dbc, tblname->mb, hfile->mb, errfile->mb, direction);
#ifdef ENABLE_ODBC_WIDE
		}
#endif
	}
	if (!tblname)
		ODBCBCP_ERROR_RETURN("HY009");

	/* Free previously allocated storage in dbproc & initialise flags, etc. */

	odbc_bcp_free_storage(dbc);

	/*
	 * Validate other parameters
	 */
	if (dbc->tds_socket->conn->tds_version < 0x500)
		ODBCBCP_ERROR_RETURN("HYC00");

	if (direction != BCP_DIRECTION_IN || hfile || errfile)
		ODBCBCP_ERROR_RETURN("HYC00");

	/* Allocate storage */

	dbc->bcpinfo = tds_alloc_bcpinfo();
	if (dbc->bcpinfo == NULL)
		ODBCBCP_ERROR_RETURN("HY001");

	if (!odbc_dstr_copy(dbc, &dbc->bcpinfo->tablename, SQL_NTS, tblname)) {
		odbc_bcp_free_storage(dbc);
		ODBCBCP_ERROR_RETURN("HY001");
	}

	if (tds_dstr_len(&dbc->bcpinfo->tablename) > 92 && !IS_TDS7_PLUS(dbc->tds_socket->conn)) { 	/* 30.30.30 */
		odbc_bcp_free_storage(dbc);
		ODBCBCP_ERROR_RETURN("HYC00");
	}

	dbc->bcpinfo->direction = direction;

	dbc->bcpinfo->xfer_init  = 0;
	dbc->bcpinfo->bind_count = 0;

	if (TDS_FAILED(tds_bcp_init(dbc->tds_socket, dbc->bcpinfo))) {
		/* TODO return proper error */
		/* Attempt to use Bulk Copy with a non-existent Server table (might be why ...) */
		ODBCBCP_ERROR_RETURN("HY000");
	}
}


/**
 * \ingroup odbc_bcp
 * \brief Set BCP options for data transfer
 *
 * \param dbc ODBC database connection object
 * \param field symbolic constant indicating the option to be set, one of:
 *  		- \b BCPKEEPIDENTITY Enable identity insert, as if by executing
 *  		  'SET IDENTITY_INSERT \a table ON'. The default is off
 *  		- \b BCPHINTS Arbitrary extra text to pass to the server. See the
 *  		  documentation for the bcp command-line tool which came with your
 *  		  database server for the correct syntax.
 * \param value The value for \a field.
 *
 * \remarks These options control the behavior of bcp_sendrow().
 *
 * \sa 	odbc_bcp_batch(), odbc_bcp_init(), odbc_bcp_done()
 */
void
odbc_bcp_control(TDS_DBC *dbc, int field, void *value)
{
	tdsdump_log(TDS_DBG_FUNC, "bcp_control(%p, %d, %p)\n", dbc, field, value);
	if (dbc->bcpinfo == NULL)
		ODBCBCP_ERROR_RETURN("HY010");


	switch (field) {
	case BCPKEEPIDENTITY:
		dbc->bcpinfo->identity_insert_on = (value != NULL);
		break;
	case BCPHINTS:
		if (!value)
			ODBCBCP_ERROR_RETURN("HY009");
		dbc->bcphint = strdup((char*)value);
		dbc->bcpinfo->hint = dbc->bcphint;
		break;
	default:
		ODBCBCP_ERROR_RETURN("HY009");
	}
}

/**
 * \ingroup odbc_bcp
 * \brief Override bcp_bind() by pointing to a different host variable.
 *
 * \param dbc ODBC database connection object
 * \param colptr The pointer, the address of your variable.
 * \param table_column The 1-based column ordinal in the table.
 * \remarks Use between calls to bcp_sendrow().  After calling bcp_colptr(),
 * 		subsequent calls to bcp_sendrow() will bind to the new address.
 * \sa 	odbc_bcp_bind(), odbc_bcp_sendrow()
 */
void
odbc_bcp_colptr(TDS_DBC *dbc, const void * colptr, int table_column)
{
	TDSCOLUMN *curcol;

	tdsdump_log(TDS_DBG_FUNC, "bcp_colptr(%p, %p, %d)\n", dbc, colptr, table_column);
	if (dbc->bcpinfo == NULL || dbc->bcpinfo->bindinfo == NULL)
		ODBCBCP_ERROR_RETURN("HY010");
	/* colptr can be NULL */

	if (dbc->bcpinfo->direction != BCP_DIRECTION_IN)
		ODBCBCP_ERROR_RETURN("HY010");
	if (table_column <= 0 || table_column > dbc->bcpinfo->bindinfo->num_cols)
		ODBCBCP_ERROR_RETURN("HY009");

	curcol = dbc->bcpinfo->bindinfo->columns[table_column - 1];
	curcol->column_varaddr = (TDS_CHAR *)colptr;
}


/**
 * \ingroup odbc_bcp
 * \brief Write data in host variables to the table.
 *
 * \param dbc ODBC database connection object
 *
 * \remarks Call bcp_bind() first to describe the variables to be used.
 *	Use bcp_batch() to commit sets of rows.
 *	After sending the last row call bcp_done().
 * \sa 	odbc_bcp_batch(), odbc_bcp_bind(), odbc_bcp_colptr(), odbc_bcp_done(),
 * 	odbc_bcp_init()
 */
void
odbc_bcp_sendrow(TDS_DBC *dbc)
{
	TDSSOCKET *tds;

	tdsdump_log(TDS_DBG_FUNC, "bcp_sendrow(%p)\n", dbc);
	if (dbc->bcpinfo == NULL)
		ODBCBCP_ERROR_RETURN("HY010");

	tds = dbc->tds_socket;

	if (dbc->bcpinfo->direction != BCP_DIRECTION_IN)
		ODBCBCP_ERROR_RETURN("HY010");

	/*
	 * The first time sendrow is called after bcp_init,
	 * there is a certain amount of initialisation to be done.
	 */
	if (dbc->bcpinfo->xfer_init == 0) {

		/* The start_copy function retrieves details of the table's columns */
		if (TDS_FAILED(tds_bcp_start_copy_in(tds, dbc->bcpinfo)))
			ODBCBCP_ERROR_RETURN("HY000");

		dbc->bcpinfo->xfer_init = 1;
	}

	dbc->bcpinfo->parent = dbc;
	if (TDS_FAILED(tds_bcp_send_record(dbc->tds_socket, dbc->bcpinfo, _bcp_get_col_data, NULL, 0)))
		ODBCBCP_ERROR_RETURN("HY000");
}


/**
 * \ingroup odbc_bcp
 * \brief Commit a set of rows to the table.
 *
 * \param dbc ODBC database connection object
 * \remarks If not called, bcp_done() will cause the rows to be saved.
 * \return Count of rows saved, or -1 on error.
 * \sa 	odbc_bcp_bind(), odbc_bcp_done(), odbc_bcp_sendrow()
 */
int
odbc_bcp_batch(TDS_DBC *dbc)
{
	int rows_copied = 0;

	tdsdump_log(TDS_DBG_FUNC, "bcp_batch(%p)\n", dbc);
	if (dbc->bcpinfo == NULL)
		ODBCBCP_ERROR_DBINT("HY010");

	if (TDS_FAILED(tds_bcp_done(dbc->tds_socket, &rows_copied)))
		ODBCBCP_ERROR_DBINT("HY000");

	tds_bcp_start(dbc->tds_socket, dbc->bcpinfo);

	return rows_copied;
}

/**
 * \ingroup odbc_bcp
 * \brief Conclude the transfer of data from program variables.
 *
 * \param dbc ODBC database connection object
 * \remarks Do not overlook this function.  According to Sybase, failure to call bcp_done()
 * "will result in unpredictable errors".
 * \return As with bcp_batch(), the count of rows saved, or -1 on error.
 * \sa 	bcp_batch(), bcp_bind(), bcp_moretext(), bcp_sendrow()
 */
int
odbc_bcp_done(TDS_DBC *dbc)
{
	int rows_copied;

	tdsdump_log(TDS_DBG_FUNC, "bcp_done(%p)\n", dbc);

	if (!(dbc->bcpinfo))
		ODBCBCP_ERROR_DBINT("HY010");

	if (TDS_FAILED(tds_bcp_done(dbc->tds_socket, &rows_copied)))
		ODBCBCP_ERROR_DBINT("HY000");

	odbc_bcp_free_storage(dbc);

	return rows_copied;
}

/**
 * \ingroup odbc_bcp
 * \brief Bind a program host variable to a database column
 *
 * \param dbc ODBC database connection object
 * \param varaddr address of host variable
 * \param prefixlen length of any prefix found at the beginning of \a varaddr, in bytes.
 *	Use zero for fixed-length datatypes.
 * \param varlen bytes of data in \a varaddr.  Zero for NULL, -1 for fixed-length datatypes.
 * \param terminator byte sequence that marks the end of the data in \a varaddr
 * \param termlen length of \a terminator
 * \param vartype datatype of the host variable
 * \param table_column Nth column, starting at 1, in the table.
 *
 * \remarks The order of operation is:
 *	- bcp_init() with \a hfile == NULL and \a direction == DB_IN.
 *	- bcp_bind(), once per column you want to write to
 *	- bcp_batch(), optionally, to commit a set of rows
 *	- bcp_done()
 *
 * \sa 	odbc_bcp_batch(), odbc_bcp_done(), odbc_bcp_sendrow()
 */
void
odbc_bcp_bind(TDS_DBC *dbc, const void * varaddr, int prefixlen, int varlen,
	 const void * terminator, int termlen, int vartype, int table_column)
{
	TDSCOLUMN *colinfo;

	tdsdump_log(TDS_DBG_FUNC, "bcp_bind(%p, %p, %d, %d -- %p, %d, %d, %d)\n",
						dbc, varaddr, prefixlen, varlen,
						terminator, termlen, vartype, table_column);
	if (!dbc->bcpinfo)
		ODBCBCP_ERROR_RETURN("HY010");

	if (dbc->bcpinfo->direction != BCP_DIRECTION_IN)
		ODBCBCP_ERROR_RETURN("HY010");

	if (varlen < -1 && varlen != SQL_VARLEN_DATA)
		ODBCBCP_ERROR_RETURN("HY009");

	if (prefixlen != 0 && prefixlen != 1 && prefixlen != 2 && prefixlen != 4 && prefixlen != 8)
		ODBCBCP_ERROR_RETURN("HY009");

	if (vartype != 0 && !is_tds_type_valid(vartype))
		ODBCBCP_ERROR_RETURN("HY004");

	if (prefixlen == 0 && varlen == SQL_VARLEN_DATA && termlen == -1 && !is_fixed_type(vartype)) {
		tdsdump_log(TDS_DBG_FUNC, "bcp_bind(): non-fixed type %d requires prefix or terminator\n", vartype);
		ODBCBCP_ERROR_RETURN("HY009");
	}

	if (table_column <= 0 ||  table_column > dbc->bcpinfo->bindinfo->num_cols)
		ODBCBCP_ERROR_RETURN("HY009");

	if (varaddr == NULL && (prefixlen != 0 || termlen != 0))
		ODBCBCP_ERROR_RETURN("HY009");

	colinfo = dbc->bcpinfo->bindinfo->columns[table_column - 1];

	/* If varaddr is NULL and varlen greater than 0, the table column type must be SYBTEXT or SYBIMAGE
		and the program variable type must be SYBTEXT, SYBCHAR, SYBIMAGE or SYBBINARY */
	if (varaddr == NULL && varlen >= 0) {
		int fOK = (colinfo->column_type == SYBTEXT || colinfo->column_type == SYBIMAGE) &&
			  (vartype == SYBTEXT || vartype == SYBCHAR || vartype == SYBIMAGE || vartype == SYBBINARY );
		if( !fOK ) {
			tdsdump_log(TDS_DBG_FUNC, "bcp_bind: SYBEBCBNTYP: column=%d and vartype=%d (should fail?)\n",
							colinfo->column_type, vartype);
			ODBCBCP_ERROR_RETURN("HY009");
		}
	}

	colinfo->column_varaddr  = (char *)varaddr;
	colinfo->column_bindtype = vartype;
	colinfo->column_bindlen  = varlen;
	colinfo->bcp_prefix_len = prefixlen;

	TDS_ZERO_FREE(colinfo->bcp_terminator);
	colinfo->bcp_term_len = 0;
	if (termlen > 0) {
		if ((colinfo->bcp_terminator =  tds_new(TDS_CHAR, termlen)) == NULL)
			ODBCBCP_ERROR_RETURN("HY001");
		memcpy(colinfo->bcp_terminator, terminator, termlen);
		colinfo->bcp_term_len = termlen;
	}
}

static SQLLEN
_bcp_iconv_helper(const TDS_DBC *dbc, const TDSCOLUMN *bindcol, const TDS_CHAR * src, size_t srclen, char * dest, size_t destlen)
{
	if (bindcol->char_conv) {
		char *orig_dest = dest;

		if (tds_iconv(dbc->tds_socket, bindcol->char_conv, to_server, &src, &srclen, &dest, &destlen) == (size_t)-1)
			return -1;
		return dest - orig_dest;
	}

	if (destlen > srclen)
		destlen = srclen;
	memcpy(dest, src, destlen);
	return destlen;
}

static SQLLEN
_tdsodbc_dbconvert(TDS_DBC *dbc, int srctype, const TDS_CHAR * src, SQLLEN src_len,
		   int desttype, unsigned char * dest, TDSCOLUMN *bindcol)
{
	CONV_RESULT dres;
	SQLLEN ret;
	SQLLEN len;
	SQLLEN destlen = bindcol->column_size;
	TDS_DATETIMEALL dta;
	TDS_NUMERIC num;
	SQL_NUMERIC_STRUCT * sql_num;
	int always_convert = 0;

	assert(src_len >= 0);
	assert(src != NULL);
	assert(dest != NULL);
	assert(destlen > 0);

	tdsdump_log(TDS_DBG_FUNC, "tdsodbc_dbconvert(%p, %d, %p, %d, %d, %p, %d)\n",
			dbc, srctype, src, (int)src_len, desttype, dest, (int)destlen);

	switch (srctype) {
	case SYBMSDATETIME2:
		convert_datetime2server(SQL_C_TYPE_TIMESTAMP, src, &dta);
		dta.time_prec = (destlen - 40) / 2;
		src = (char *) &dta;
		break;
	case SYBDECIMAL:
	case SYBNUMERIC:
		sql_num = (SQL_NUMERIC_STRUCT *) src;
		num.precision = sql_num->precision;
		num.scale = sql_num->scale;
		num.array[0] = sql_num->sign ^ 1;
		/* test precision so client do not crash our library */
		if (num.precision <= 0 || num.precision > 38 || num.scale > num.precision)
			/* TODO add proper error */
			return -1;
		len = tds_numeric_bytes_per_prec[num.precision];
		memcpy(num.array + 1, sql_num->val, len - 1);
		tds_swap_bytes(num.array + 1, len - 1);
		if (len < sizeof(num.array))
			memset(num.array + len, 0, sizeof(num.array) - len);
		src = (char *) &num;
		always_convert = num.scale != bindcol->column_scale;
		break;
		/* TODO intervals */
	}

	/* oft times we are asked to convert a data type to itself */
	if ((srctype == desttype || is_similar_type(srctype, desttype)) && !always_convert) {
		if (is_char_type(desttype)) {
			ret = _bcp_iconv_helper(dbc, bindcol, src, src_len, (char *)dest, destlen);
		}
		else {
			ret = destlen < src_len ? destlen : src_len;
			memcpy(dest, src, ret);
		}
		return ret;
	}

	tdsdump_log(TDS_DBG_INFO1, "dbconvert() calling tds_convert\n");

	if (is_numeric_type(desttype)) {
		dres.n.precision = bindcol->column_prec;
		dres.n.scale = bindcol->column_scale;
	}
	len = tds_convert(dbc->env->tds_ctx, srctype, src, src_len, desttype, &dres);
	tdsdump_log(TDS_DBG_INFO1, "dbconvert() called tds_convert returned %d\n", (int)len);

	if (len < 0) {
		odbc_convert_err_set(&dbc->errs, len);
		return -1;
	}

	switch (desttype) {
	case SYBBINARY:
	case SYBVARBINARY:
	case SYBIMAGE:
		ret = destlen < len ? destlen : len;
		memcpy(dest, dres.ib, ret);
		free(dres.ib);
		break;
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBINT8:
	case SYBFLT8:
	case SYBREAL:
	case SYBBIT:
	case SYBBITN:
	case SYBMONEY:
	case SYBMONEY4:
	case SYBDATETIME:
	case SYBDATETIME4:
	case SYBNUMERIC:
	case SYBDECIMAL:
	case SYBUNIQUE:
	case SYBMSDATE:
	case SYBMSTIME:
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
		memcpy(dest, &dres, len);
		ret = len;
		break;
	case SYBCHAR:
	case SYBVARCHAR:
	case SYBTEXT:
		ret = _bcp_iconv_helper(dbc, bindcol, dres.c, len, (char *)dest, destlen);
		free(dres.c);
		break;
	default:
		tdsdump_log(TDS_DBG_INFO1, "error: dbconvert(): unrecognized desttype %d \n", desttype);
		ret = -1;
		break;

	}
	return (ret);
}

static TDSRET
_bcp_get_col_data(TDSBCPINFO *bcpinfo, TDSCOLUMN *bindcol, int offset)
{
	TDS_TINYINT ti;
	TDS_SMALLINT si;
	TDS_INT li;
	tds_sysdep_int64_type lli;
	TDS_SERVER_TYPE desttype, coltype;
	SQLLEN col_len;
	int data_is_null;
	SQLLEN bytes_read;
	int converted_data_size;
	TDS_CHAR *dataptr;
	TDS_DBC *dbc = (TDS_DBC *) bcpinfo->parent;

	tdsdump_log(TDS_DBG_FUNC, "_bcp_get_col_data(%p, %p)\n", bcpinfo, bindcol);

	dataptr = bindcol->column_varaddr;

	data_is_null = 0;
	col_len = SQL_NULL_DATA;

	/* If a prefix length specified, read the correct  amount of data. */

	if (bindcol->bcp_prefix_len > 0) {

		switch (bindcol->bcp_prefix_len) {
		case 1:
			memcpy(&ti, dataptr, 1);
			dataptr += 1;
			col_len = ti;
			break;
		case 2:
			memcpy(&si, dataptr, 2);
			dataptr += 2;
			col_len = si;
			break;
		case 4:
			memcpy(&li, dataptr, 4);
			dataptr += 4;
			col_len = li;
			break;
		case 8:
			memcpy(&lli, dataptr, 8);
			dataptr += 8;
			col_len = lli;
			if (lli != col_len)
				return TDS_FAIL;
			break;
		}
		if (col_len == SQL_NULL_DATA)
			data_is_null = 1;
	}

	/* if (Max) column length specified take that into consideration. */

	if (bindcol->column_bindlen == SQL_NULL_DATA)
		data_is_null = 1;
	else if (!data_is_null && bindcol->column_bindlen != SQL_VARLEN_DATA) {
		if (col_len != SQL_NULL_DATA)
			col_len = ((tds_sysdep_int64_type)bindcol->column_bindlen < col_len) ? bindcol->column_bindlen : col_len;
		else
			col_len = bindcol->column_bindlen;
	}

	desttype = tds_get_conversion_type(bindcol->column_type, bindcol->column_size);

	/* Fixed Length data - this overrides anything else specified */
	coltype = bindcol->column_bindtype == 0 ? desttype : (TDS_SERVER_TYPE) bindcol->column_bindtype;
	if (is_fixed_type(coltype)) {
		col_len = tds_get_size_by_type(coltype);
	}

	/* read the data, finally */

	if (!data_is_null && bindcol->bcp_term_len > 0) {	/* terminated field */
		bytes_read = _bcp_get_term_var(dataptr, bindcol->bcp_terminator, bindcol->bcp_term_len);

		if (col_len != SQL_NULL_DATA)
			col_len = (bytes_read < col_len) ? bytes_read : col_len;
		else
			col_len = bytes_read;
	}

	if (data_is_null) {
		bindcol->bcp_column_data->datalen = 0;
		bindcol->bcp_column_data->is_null = 1;
	} else {
		if ((converted_data_size =
		     _tdsodbc_dbconvert(dbc, coltype,
			       dataptr, col_len,
			       desttype, bindcol->bcp_column_data->data, bindcol)) == -1) {
			return TDS_FAIL;
		}

		bindcol->bcp_column_data->datalen = converted_data_size;
		bindcol->bcp_column_data->is_null = 0;
	}

	return TDS_SUCCESS;
}

/**
 * Get the data for bcp-in from program variables, where the program data
 * have been identified as character terminated,
 * This is a low-level, internal function.  Call it correctly.
 */
static SQLLEN
_bcp_get_term_var(const TDS_CHAR * pdata, const TDS_CHAR * term, int term_len)
{
	SQLLEN bufpos;

	assert(term_len > 0);

	if (term_len == 1 && *term == '\0')  /* significant optimization for very common case */
		return strlen(pdata);

	/* if bufpos becomes negative, we probably failed to find the terminator */
	for (bufpos = 0; bufpos >= 0 && memcmp(pdata, term, term_len) != 0; pdata++) {
		bufpos++;
	}

	assert(bufpos >= 0);
	return bufpos;
}

void
odbc_bcp_free_storage(TDS_DBC *dbc)
{
	tdsdump_log(TDS_DBG_FUNC, "_bcp_free_storage(%p)\n", dbc);
	assert(dbc);

	tds_free_bcpinfo(dbc->bcpinfo);
	dbc->bcpinfo = NULL;
	TDS_ZERO_FREE(dbc->bcphint);
}

