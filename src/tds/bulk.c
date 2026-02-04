/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2008-2010  Frediano Ziglio
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
 * \brief Handle bulk copy
 */

#include <config.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <assert.h>

#include <freetds/tds.h>
#include <freetds/checks.h>
#include <freetds/bytes.h>
#include <freetds/iconv.h>
#include <freetds/stream.h>
#include <freetds/convert.h>
#include <freetds/utils/string.h>
#include <freetds/replacements.h>
#include <freetds/enum_cap.h>

/**
 * Holds clause buffer
 */
typedef struct tds_pbcb
{
	/** buffer */
	char *pb;
	/** buffer length */
	unsigned int cb;
	/** true is buffer came from malloc */
	bool from_malloc;
} TDSPBCB;

static TDSRET tds7_bcp_send_colmetadata(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);
static TDSRET tds_bcp_start_insert_stmt(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);
static int tds5_bcp_add_fixed_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error,
				      int offset, unsigned char * rowbuffer, int start);
static int tds5_bcp_add_variable_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error,
					 int offset, TDS_UCHAR *rowbuffer, int start, int *pncols);
static void tds_bcp_row_free(TDSRESULTINFO* result, unsigned char *row);
static TDSRET tds5_process_insert_bulk_reply(TDSSOCKET * tds, TDSBCPINFO *bcpinfo);
static TDSRET probe_sap_locking(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);

/* Sybase bulk column data */
static TDS5COLINFO* sybase_colinfo_alloc(int n_cols)
{
	return calloc(n_cols, sizeof(TDS5COLINFO));
}

/**
 * Initialize BCP information.
 * Query structure of the table to server.
 * \tds
 * \param bcpinfo BCP information to initialize. Structure should be allocate
 *        and table name and direction should be already set.
 */
TDSRET
tds_bcp_init(TDSSOCKET *tds, TDSBCPINFO *bcpinfo)
{
	TDSRESULTINFO *resinfo;
	TDSRESULTINFO *bindinfo = NULL;
	TDSCOLUMN *curcol;
	TDS_INT result_type;
	int i;
	TDSRET rc;
	const char *fmt;

	/* FIXME don't leave state in processing state */

	/* Check table locking type. Do this first, because the code in bcp_init() which
	 * calls us seems to depend on the information from the columns query still being
	 * active in the context.
	 */
	probe_sap_locking(tds, bcpinfo);

	/* TODO quote tablename if needed */
	if (bcpinfo->direction != TDS_BCP_QUERYOUT)
		fmt = "SET FMTONLY ON select * from %s SET FMTONLY OFF";
	else
		fmt = "SET FMTONLY ON %s SET FMTONLY OFF";

	if (TDS_FAILED(rc=tds_submit_queryf(tds, fmt, tds_dstr_cstr(&bcpinfo->tablename))))
		/* TODO return an error ?? */
		/* Attempt to use Bulk Copy with a non-existent Server table (might be why ...) */
		return rc;

	/* TODO possibly stop at ROWFMT and copy before going to idle */
	/* TODO check what happen if table is not present, cleanup on error */
	while ((rc = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS))
		   == TDS_SUCCESS)
		continue;
	TDS_PROPAGATE(rc);

	/* copy the results info from the TDS socket */
	if (!tds->res_info)
		return TDS_FAIL;

	resinfo = tds->res_info;
	if ((bindinfo = tds_alloc_results(resinfo->num_cols)) == NULL) {
		rc = TDS_FAIL;
		goto cleanup;
	}

	/* This is the size of our internal buffer for storing the unpacked row
	 * data. It may differ from the actual BCP packed data size, for example
	 * we unpack date/time values into structures like TDS_DATETIMEALL.
	 */
	bindinfo->row_size = resinfo->row_size;
	tdsdump_log(TDS_DBG_INFO1, "[BCP] Internal row buffer size is %d\n", bindinfo->row_size);

	/* Copy the column metadata */
	rc = TDS_FAIL;
	for (i = 0; i < bindinfo->num_cols; i++) {

		curcol = bindinfo->columns[i];
		
		/*
		 * TODO use memcpy ??
		 * curcol and resinfo->columns[i] are both TDSCOLUMN.  
		 * Why not "curcol = resinfo->columns[i];"?  Because the rest of TDSCOLUMN (below column_timestamp)
		 * isn't being used.  Perhaps this "upper" part of TDSCOLUMN should be a substructure.
		 * Or, see if the "lower" part is unused (and zeroed out) at this point, and just do one assignment.
		 */
		curcol->funcs = resinfo->columns[i]->funcs;
		curcol->column_type = resinfo->columns[i]->column_type;
		curcol->column_usertype = resinfo->columns[i]->column_usertype;
		curcol->column_flags = resinfo->columns[i]->column_flags;
		if (curcol->column_varint_size == 0)
			curcol->column_cur_size = resinfo->columns[i]->column_cur_size;
		else
			curcol->column_cur_size = -1;
		curcol->column_size = resinfo->columns[i]->column_size;
		curcol->column_varint_size = resinfo->columns[i]->column_varint_size;
		curcol->column_prec = resinfo->columns[i]->column_prec;
		curcol->column_scale = resinfo->columns[i]->column_scale;
		curcol->on_server = resinfo->columns[i]->on_server;
		curcol->char_conv = resinfo->columns[i]->char_conv;
		if (!tds_dstr_dup(&curcol->column_name, &resinfo->columns[i]->column_name))
			goto cleanup;
		if (!tds_dstr_dup(&curcol->table_column_name, &resinfo->columns[i]->table_column_name))
			goto cleanup;
		curcol->column_nullable = resinfo->columns[i]->column_nullable;
		curcol->column_identity = resinfo->columns[i]->column_identity;
		curcol->column_timestamp = resinfo->columns[i]->column_timestamp;
		curcol->column_computed = resinfo->columns[i]->column_computed;
		
		memcpy(curcol->column_collation, resinfo->columns[i]->column_collation, 5);
		curcol->use_iconv_out = 0;

		/* From MS documentation:
		 * Note that for INSERT BULK operations, XMLTYPE is to be sent as NVARCHAR(N) or NVARCHAR(MAX)
		 * data type. An error is produced if XMLTYPE is specified.
		 */
		if (curcol->on_server.column_type == SYBMSXML) {
			curcol->on_server.column_type = XSYBNVARCHAR;
			curcol->column_type = SYBVARCHAR;
			memcpy(curcol->column_collation, tds->conn->collation, 5);
		}

		if (is_numeric_type(curcol->column_type)) {
			curcol->bcp_column_data = tds_alloc_bcp_column_data(sizeof(TDS_NUMERIC));
			((TDS_NUMERIC *) curcol->bcp_column_data->data)->precision = curcol->column_prec;
			((TDS_NUMERIC *) curcol->bcp_column_data->data)->scale = curcol->column_scale;
		} else {
			curcol->bcp_column_data = 
				tds_alloc_bcp_column_data(TDS_MAX(curcol->column_size,curcol->on_server.column_size));
		}
		if (!curcol->bcp_column_data)
			goto cleanup;
	}

	/* bindinfo->current_row is used to hold TDS5 BCP row packing buffer, we will
	 * allocate some space later once we know how big the row will be. 
	 * Not to be confused with the same members of "resinfo"
	 * in a normal (non-bcp) query,  which holds unpacked row data.
	 */
	bindinfo->current_row = NULL;
	bindinfo->row_size = 0;

	if (bcpinfo->identity_insert_on) {

		rc = tds_submit_queryf(tds, "set identity_insert %s on", tds_dstr_cstr(&bcpinfo->tablename));
		if (TDS_FAILED(rc))
			goto cleanup;

		/* TODO use tds_process_simple_query */
		while ((rc = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS))
			   == TDS_SUCCESS) {
		}
		if (rc != TDS_NO_MORE_RESULTS)
			goto cleanup;
	}

	bcpinfo->bindinfo = bindinfo;
	bcpinfo->bind_count = 0;

	return TDS_SUCCESS;

cleanup:
	tds_free_results(bindinfo);
	return rc;
}

/**
 * Detect if table we're writing to uses 'datarows' lockmode.
 * \tds
 * \param bcpinfo BCP information already prepared
 * \return TDS_SUCCESS or TDS_FAIL.
 */
static TDSRET
probe_sap_locking(TDSSOCKET *tds, TDSBCPINFO *bcpinfo)
{
	TDSRET rc;
	unsigned int value;
	bool value_found;
	TDS_INT resulttype;
	const char *full_tablename, *rdot, *tablename;

	/* Only needed for inward data */
	if (bcpinfo->direction != TDS_BCP_IN)
		return TDS_SUCCESS;

	/* Only needed for SAP ASE versions which support datarows-locking. */
	if (!TDS_IS_SYBASE(tds) || !tds_capability_has_req(tds->conn, TDS_REQ_DOL_BULK))
		return TDS_SUCCESS;

	/* A request to probe database.owner.tablename needs to check database.owner.sysobjects for tablename
	 * (it doesn't work to check sysobjects for database.owner.tablename) */
	full_tablename = tds_dstr_cstr(&bcpinfo->tablename);
	rdot = strrchr(full_tablename, '.');

	if (rdot != NULL)
		tablename = rdot + 1;
	else
		tablename = full_tablename;

	/* Temporary tables attributes are stored differently */
	if (rdot == NULL && tablename[0] == '#')
	{
		TDS_PROPAGATE(tds_submit_queryf(tds,
			"select sysstat2 from tempdb..sysobjects where id = object_id('tempdb..%s')",
			tablename));
	}
	else
	{
		TDS_PROPAGATE(tds_submit_queryf(tds, "select sysstat2 from %.*ssysobjects where type='U' and name='%s'",
			(int)(rdot ? (rdot - full_tablename + 1) : 0), full_tablename, tablename));
	}

	value = 0;
	value_found = false;

	while ((rc = tds_process_tokens(tds, &resulttype, NULL, TDS_TOKEN_RESULTS)) == TDS_SUCCESS) {
		const unsigned int stop_mask = TDS_RETURN_DONE | TDS_RETURN_ROW;

		if (resulttype != TDS_ROW_RESULT)
			continue;

		/* We must keep processing result tokens (even if we've found what we're looking for) so that the
		 * stream is ready for subsequent queries. */
		while ((rc = tds_process_tokens(tds, &resulttype, NULL, stop_mask)) == TDS_SUCCESS) {
			TDSCOLUMN *col;
			TDS_SERVER_TYPE ctype;
			CONV_RESULT dres;
			TDS_INT res;

			if (resulttype != TDS_ROW_RESULT)
				break;

			/* Get INT4 from column 0 */
			if (!tds->current_results || tds->current_results->num_cols < 1)
				continue;

			col = tds->current_results->columns[0];
			if (col->column_cur_size < 0)
				continue;

			ctype = tds_get_conversion_type(col->column_type, col->column_size);
			res = tds_convert(tds_get_ctx(tds), ctype, col->column_data, col->column_cur_size, SYBINT4, &dres);
			if (res < 0)
				continue;

			value = dres.i;
			value_found = true;
		}
	}
	TDS_PROPAGATE(rc);

	/* No valid result - Treat this as meaning the feature is lacking; it could be an old Sybase version for example */
	if (!value_found) {
		tdsdump_log(TDS_DBG_INFO1, "[DOL BULK] No valid result returned by probe.\n");
		return TDS_SUCCESS;
	}

	/* Log and analyze result.
	 * Sysstat2 flag values:
	 *    https://infocenter.sybase.com/help/index.jsp?topic=/com.sybase.infocenter.help.ase.16.0/doc/html/title.html
	 * 0x08000 - datarows locked
	 * 0x20000 - clustered index present. (Not recommended for performance reasons to datarows-lock with clustered index) */
	tdsdump_log(TDS_DBG_INFO1, "%x = sysstat2 for '%s'", value, full_tablename);

	if (0x8000 & value) {
		bcpinfo->datarows_locking = true;
		tdsdump_log(TDS_DBG_INFO1, "Table has datarows-locking; enabling DOL BULK format.\n");

		if (0x20000 & value)
			tdsdump_log(TDS_DBG_WARN, "Table also has clustered index: bulk insert performance may be degraded.\n");
	}
	return TDS_SUCCESS;
}

/**
 * Help to build query to be sent to server.
 * Append column declaration to the query.
 * Only for TDS 7.0+.
 * \tds
 * \param[out] clause output string
 * \param bcpcol column to append
 * \param first  true if column is the first
 * \return TDS_SUCCESS or TDS_FAIL.
 */
static TDSRET
tds7_build_bulk_insert_stmt(TDSSOCKET * tds, TDSPBCB * clause, TDSCOLUMN * bcpcol, int first)
{
	char column_type[128];

	tdsdump_log(TDS_DBG_FUNC, "tds7_build_bulk_insert_stmt(%p, %p, %p, %d)\n", tds, clause, bcpcol, first);

	if (TDS_FAILED(tds_get_column_declaration(tds, bcpcol, column_type))) {
		tdserror(tds_get_ctx(tds), tds, TDSEBPROBADTYP, errno);
		tdsdump_log(TDS_DBG_FUNC, "error: cannot build bulk insert statement. unrecognized server datatype %d\n",
			    bcpcol->on_server.column_type);
		return TDS_FAIL;
	}

	if (IS_TDS71_PLUS(tds->conn) && bcpcol->char_conv) {
		strcat(column_type, " COLLATE ");
		strcat(column_type, tds_canonical_collate_name(bcpcol->char_conv->to.charset.canonic));
	}

	if (clause->cb < strlen(clause->pb)
	    + tds_quote_id(tds, NULL, tds_dstr_cstr(&bcpcol->column_name), tds_dstr_len(&bcpcol->column_name))
	    + strlen(column_type)
	    + ((first) ? 2u : 4u)) {
		char *temp = tds_new(char, 2 * clause->cb);

		if (!temp) {
			tdserror(tds_get_ctx(tds), tds, TDSEMEM, errno);
			return TDS_FAIL;
		}
		strcpy(temp, clause->pb);
		if (clause->from_malloc)
			free(clause->pb);
		clause->from_malloc = true;
		clause->pb = temp;
		clause->cb *= 2;
	}

	if (!first)
		strcat(clause->pb, ", ");

	tds_quote_id(tds, strchr(clause->pb, 0), tds_dstr_cstr(&bcpcol->column_name), tds_dstr_len(&bcpcol->column_name));
	strcat(clause->pb, " ");
	strcat(clause->pb, column_type);

	return TDS_SUCCESS;
}

/**
 * Prepare the query to be sent to server to request BCP information
 * \tds
 * \param bcpinfo BCP information
 */
static TDSRET
tds_bcp_start_insert_stmt(TDSSOCKET * tds, TDSBCPINFO * bcpinfo)
{
	char *query;

	if (IS_TDS7_PLUS(tds->conn)) {
		int i, firstcol, erc;
		char *hint;
		TDSCOLUMN *bcpcol;
		TDSPBCB colclause;
		char clause_buffer[4096] = { 0 };

		colclause.pb = clause_buffer;
		colclause.cb = sizeof(clause_buffer);
		colclause.from_malloc = false;

		/* TODO avoid asprintf, use always malloc-ed buffer */
		firstcol = 1;

		for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
			bcpcol = bcpinfo->bindinfo->columns[i];

			if (bcpcol->column_timestamp)
				continue;
			if (!bcpinfo->identity_insert_on && bcpcol->column_identity)
				continue;
			if (bcpcol->column_computed)
				continue;
			tds7_build_bulk_insert_stmt(tds, &colclause, bcpcol, firstcol);
			firstcol = 0;
		}

		if (!tds_dstr_isempty(&bcpinfo->hint)) {
			if (asprintf(&hint, " with (%s)", tds_dstr_cstr(&bcpinfo->hint)) < 0)
				hint = NULL;
		} else {
			hint = strdup("");
		}
		if (!hint) {
			if (colclause.from_malloc)
				TDS_ZERO_FREE(colclause.pb);
			return TDS_FAIL;
		}

		erc = asprintf(&query, "insert bulk %s (%s)%s", tds_dstr_cstr(&bcpinfo->tablename), colclause.pb, hint);

		free(hint);
		if (colclause.from_malloc)
			TDS_ZERO_FREE(colclause.pb);	/* just for good measure; not used beyond this point */

		if (erc < 0)
			return TDS_FAIL;
	} else {
		/* NOTE: Current ASE docs do not mention "insert bulk", it might be deprecated? */
		/* NOTE: if we use "with nodescribe" for following inserts server do not send describe */
		if (asprintf(&query, "insert bulk %s", tds_dstr_cstr(&bcpinfo->tablename)) < 0)
			return TDS_FAIL;
	}

	/* save the statement for later... */
	bcpinfo->insert_stmt = query;

	return TDS_SUCCESS;
}

static TDSRET
tds7_send_record(TDSSOCKET *tds, TDSBCPINFO *bcpinfo,
		 tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset)
{
	int i;

	tds_put_byte(tds, TDS_ROW_TOKEN);   /* 0xd1 */
	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {

		TDS_INT save_size;
		unsigned char *save_data;
		TDSBLOB blob;
		TDSCOLUMN  *bindcol;
		TDSRET rc;

		bindcol = bcpinfo->bindinfo->columns[i];

		/*
		 * Don't send the (meta)data for timestamp columns or
		 * identity columns unless indentity_insert is enabled.
		 */

		if ((!bcpinfo->identity_insert_on && bindcol->column_identity) ||
			bindcol->column_timestamp ||
			bindcol->column_computed) {
			continue;
		}

		rc = get_col_data(bcpinfo, bindcol, offset);
		if (TDS_FAILED(rc)) {
			tdsdump_log(TDS_DBG_INFO1, "get_col_data (column %d) failed\n", i + 1);
			return rc;
		}
		tdsdump_log(TDS_DBG_INFO1, "gotten column %d length %d null %d\n",
				i + 1, bindcol->bcp_column_data->datalen, bindcol->bcp_column_data->is_null);

		save_size = bindcol->column_cur_size;
		save_data = bindcol->column_data;
		assert(bindcol->column_data == NULL);
		if (bindcol->bcp_column_data->is_null) {
			if (!bindcol->column_nullable && !is_nullable_type(bindcol->on_server.column_type)) {
				if (null_error)
					null_error(bcpinfo, i, offset);
				return TDS_FAIL;
			}
			bindcol->column_cur_size = -1;
		} else if (is_blob_col(bindcol)) {
			bindcol->column_cur_size = bindcol->bcp_column_data->datalen;
			memset(&blob, 0, sizeof(blob));
			blob.textvalue = (TDS_CHAR *) bindcol->bcp_column_data->data;
			bindcol->column_data = (unsigned char *) &blob;
		} else {
			bindcol->column_cur_size = bindcol->bcp_column_data->datalen;
			bindcol->column_data = bindcol->bcp_column_data->data;
		}
		rc = bindcol->funcs->put_data(tds, bindcol, 1);
		bindcol->column_cur_size = save_size;
		bindcol->column_data = save_data;

		TDS_PROPAGATE(rc);
	}
	return TDS_SUCCESS;
}

static TDSRET
tds5_send_record(TDSSOCKET *tds, TDSBCPINFO *bcpinfo,
		 tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset)
{
	int row_pos = 0;
	int row_sz_pos;
	int blob_cols = 0;
	int var_cols_pos;
	int var_cols_written = 0;
	TDS_INT	 old_record_size = bcpinfo->bindinfo->row_size;
	unsigned char *record = bcpinfo->bindinfo->current_row;
	int i;

	/* Zero the buffer, because some data types only write part of their fixed size */
	memset(record, '\0', old_record_size);

	/* SAP ASE Datarows-locked tables expect additional 4 blank bytes before everything else */
	if (bcpinfo->datarows_locking)
		row_pos += 4;

	/*
	 * offset 0 = number of var columns
	 * offset 1 = row number.  zeroed (datasever assigns)
	 */
	var_cols_pos = row_pos;
	row_pos += 2;

	if ((row_pos = tds5_bcp_add_fixed_columns(bcpinfo, get_col_data, null_error, offset, record, row_pos)) < 0)
		return TDS_FAIL;

	row_sz_pos = row_pos;

	/* potential variable columns to write */

	row_pos = tds5_bcp_add_variable_columns(bcpinfo, get_col_data, null_error, offset, record, row_pos, &var_cols_written);
	if (row_pos < 0)
		return TDS_FAIL;

	if (row_pos > old_record_size)
	{
		/* Reaching this means we already wrote out of bounds of "record" */
		tdsdump_log(TDS_DBG_ERROR, "Adding columns wrote %d bytes, exceeding buffer size of %d bytes\n",
			row_pos, old_record_size);
		return TDS_FAIL;
	}

	if (var_cols_written) {
		TDS_PUT_UA2LE(&record[row_sz_pos], row_pos);
		record[var_cols_pos] = var_cols_written;
	}

	tdsdump_log(TDS_DBG_INFO1, "old_record_size = %d new size = %d \n", old_record_size, row_pos);

	tds_put_smallint(tds, row_pos);
	tds_put_n(tds, record, row_pos);

	/* row is done, now handle any text/image data */

	blob_cols = 0;

	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		TDSCOLUMN  *bindcol = bcpinfo->bindinfo->columns[i];
		if (is_blob_type(bindcol->on_server.column_type)) {
			TDS_PROPAGATE(get_col_data(bcpinfo, bindcol, offset));
			/* unknown but zero */
			tds_put_smallint(tds, 0);
			TDS_PUT_BYTE(tds, bindcol->on_server.column_type);
			tds_put_byte(tds, 0xff - blob_cols);
			/*
			 * offset of txptr we stashed during variable
			 * column processing
			 */
			tds_put_smallint(tds, bindcol->column_textpos);
			tds_put_int(tds, bindcol->bcp_column_data->datalen);
			tds_put_n(tds, bindcol->bcp_column_data->data, bindcol->bcp_column_data->datalen);
			blob_cols++;

		}
	}
	return TDS_SUCCESS;
}

/**
 * Send one row of data to server
 * \tds
 * \param bcpinfo BCP information
 * \param get_col_data function to call to retrieve data to be sent
 * \param ignored function to call if we try to send NULL if not allowed (not used)
 * \param offset passed to get_col_data and null_error to specify the row to get
 * \return TDS_SUCCESS or TDS_FAIL.
 */
TDSRET
tds_bcp_send_record(TDSSOCKET *tds, TDSBCPINFO *bcpinfo,
		    tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset)
{
	TDSRET rc;

	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_send_bcp_record(%p, %p, %p, %p, %d)\n",
		    tds, bcpinfo, get_col_data, null_error, offset);

	if (tds->out_flag != TDS_BULK || tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	if (IS_TDS7_PLUS(tds->conn))
		rc = tds7_send_record(tds, bcpinfo, get_col_data, null_error, offset);
	else
		rc = tds5_send_record(tds, bcpinfo, get_col_data, null_error, offset);

	tds_set_state(tds, TDS_SENDING);
	return rc;
}

static inline void
tds5_swap_data(const TDSCOLUMN *col TDS_UNUSED, void *p TDS_UNUSED)
{
#ifdef WORDS_BIGENDIAN
	tds_swap_datatype(tds_get_conversion_type(col->on_server.column_type, col->column_size), p);
#endif
}

static TDS_INT column_bcp_max_size(TDSCOLUMN* bcpcol)
{
	/*
	 * work out maximum storage required for this datatype
	 * (in the packed BCP row, that is, excluding blob bodies)
	 * rest can be taken from the server
	 *
	 * Blobs use a 16 byte "pointer" in the row data and then
	 * send the blob body after the row.
	 *
	 * Numerics may vary in size. ASE doesn't have any variable
	 * precision datetime types, only variable precision numerics.
	 */
	if (is_blob_type(bcpcol->on_server.column_type))
		return 16;
	else if (is_numeric_type(bcpcol->on_server.column_type))
		return tds_numeric_bytes_per_prec[bcpcol->column_prec];
	else
		return bcpcol->column_size;
	/* note: column_size should not be negative (unlike column_cur_size) */
}

/**
 * Add fixed size columns to the row
 * \param bcpinfo BCP information
 * \param get_col_data function to call to retrieve data to be sent
 * \param ignored function to call if we try to send NULL if not allowed (not used)
 * \param offset passed to get_col_data and null_error to specify the row to get
 * \param rowbuffer row buffer to write to
 * \param start row buffer last end position
 * \returns new row length or -1 on error.
 */
static int
tds5_bcp_add_fixed_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error,
			   int offset, unsigned char * rowbuffer, int start)
{
	TDS_NUMERIC *num;
	int row_pos = start;
	TDS_INT i;
	int bitleft = 0, bitpos = 0;

	assert(bcpinfo);
	assert(rowbuffer);

	tdsdump_log(TDS_DBG_FUNC, "tds5_bcp_add_fixed_columns(%p, %p, %p, %d, %p, %d)\n",
		    bcpinfo, get_col_data, null_error, offset, rowbuffer, start);

	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		TDSCOLUMN *const bcpcol = bcpinfo->bindinfo->columns[i];
		TDS5COLINFO* const sycol = i < bcpinfo->sybase_count ? &bcpinfo->sybase_colinfo[i] : NULL;
		const TDS_INT column_size = bcpcol->on_server.column_size;
		const TDS_INT expect = column_bcp_max_size(bcpcol);

		/* if possible check information from server */
		if (sycol) {
			if (sycol->offset < 0)
				continue;
		} else {
			if (is_nullable_type(bcpcol->on_server.column_type) || bcpcol->column_nullable)
				continue;
		}

		tdsdump_log(TDS_DBG_FUNC, "tds5_bcp_add_fixed_columns column %d (%s) is a fixed column\n", i + 1, tds_dstr_cstr(&bcpcol->column_name));

		/* Load the column data into bcpcol->column_data */
		if (TDS_FAILED(get_col_data(bcpinfo, bcpcol, offset))) {
			tdsdump_log(TDS_DBG_INFO1, "get_col_data (column %d) failed\n", i + 1);
			return -1;
		}

		/* If data was null and we have a default value, apply it */
		if (bcpcol->bcp_column_data->is_null && sycol && sycol->has_default )
		{
			tdsdump_log(TDS_DBG_INFO1, "tds5_bcp_add_fixed_columns column %d applying default value\n", i + 1);
#ifdef ENABLE_EXTRA_CHECKS
			/* Sanity checks - Should not be possible due to the checks
			 * in process_defaults_row(). */
			if (!sycol->default_value.data || sycol->default_type != bcpcol->column_type
				|| column_size != sycol->default_value.datalen )
			{
				tdsdump_log(TDS_DBG_ERROR, "column %d default (%p) type %d size %d not match server type %d size %d\n",
					i + 1, (void*)sycol->default_value.data,
					sycol->default_type, (int)sycol->default_value.datalen,
					bcpcol->column_type, column_size);
				return -1;
			}
#endif
			memcpy(&rowbuffer[row_pos], sycol->default_value.data, column_size);
			row_pos += column_size;
			continue;
		}

		/* We have no way to send a NULL at this point, return error to client */
		if (bcpcol->bcp_column_data->is_null) {
			tdsdump_log(TDS_DBG_ERROR, "tds5_bcp_add_fixed_columns column %d is a null column\n", i + 1);
			/* No value or default value available and NULL not allowed. */
			if (null_error)
				null_error(bcpinfo, i, offset);
			return -1;
		}

		if (is_numeric_type(bcpcol->on_server.column_type)) {
			TDS_INT cpbytes;
			num = (TDS_NUMERIC *) bcpcol->bcp_column_data->data;
			cpbytes = tds_numeric_bytes_per_prec[num->precision];
			if (cpbytes > expect)
			{
				tdsdump_log(TDS_DBG_ERROR, "Fixed numeric column %d packs size %d, but we expected %d\n", i, cpbytes , expect);
				return -1;
			}
			memcpy(&rowbuffer[row_pos], num->array, cpbytes);
		} else if (bcpcol->column_type == SYBBIT) {
			/* all bit are collapsed together */
			if (!bitleft) {
				bitpos = row_pos++;
				bitleft = 8;
				rowbuffer[bitpos] = 0;
			}
			if (bcpcol->bcp_column_data->data[0])
				rowbuffer[bitpos] |= 256 >> bitleft;
			--bitleft;
			continue;
		} else {
			/* note: this branch also includes BLOBs, where column_size
			 * is the logical data size, but bcp_column_data is the
			 * 16-byte "pointer" */
			TDS_INT cpbytes;
			cpbytes = bcpcol->bcp_column_data->datalen > column_size ?
				  column_size : bcpcol->bcp_column_data->datalen;
			if (cpbytes > expect)	/* Would cause buffer overflow */
			{
				tdsdump_log(TDS_DBG_ERROR, "Fixed column %d packs size %d, but we expected %d\n", i, cpbytes, expect);
				return -1;
			}
			memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, cpbytes);
			tds5_swap_data(bcpcol, &rowbuffer[row_pos]);

			/* CHAR data may need padding out to the database length with blanks */
			/* TODO check binary !!! */
			if (bcpcol->column_type == SYBCHAR && cpbytes < column_size)
				memset(rowbuffer + row_pos + cpbytes, ' ', column_size - cpbytes);
		}

		row_pos += column_size;
	}
	return row_pos;
}

/**
 * Add variable size columns to the row
 *
 * \param bcpinfo BCP information already prepared
 * \param get_col_data function to call to retrieve data to be sent
 * \param null_error function to call if we try to send NULL if not allowed
 * \param offset passed to get_col_data and null_error to specify the row to get
 * \param rowbuffer The row image that will be sent to the server. 
 * \param start Where to begin copying data into the rowbuffer. 
 * \param pncols Address of output variable holding the count of columns added to the rowbuffer.  
 * 
 * \return length of (potentially modified) rowbuffer, or -1.
 */
static int
tds5_bcp_add_variable_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error,
			      int offset, TDS_UCHAR* rowbuffer, int start, int *pncols)
{
	TDS_USMALLINT offsets[256];
	TDS_INT i, ncols = 0;
	unsigned int row_pos;

	assert(bcpinfo);
	assert(rowbuffer);
	assert(pncols);

	tdsdump_log(TDS_DBG_FUNC, "%4s %8s %18s %18s %8s\n", 	"col", 
								"type", 
								"is_nullable_type", 
								"column_nullable", 
								"is null" );
	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		TDSCOLUMN *bcpcol = bcpinfo->bindinfo->columns[i];
		tdsdump_log(TDS_DBG_FUNC, "%4d %8d %18s %18s %8s\n", 	i,
									bcpcol->on_server.column_type,
									is_nullable_type(bcpcol->on_server.column_type)? "yes" : "no",
									bcpcol->column_nullable? "yes" : "no",
									bcpcol->bcp_column_data->is_null? "yes" : "no" );
	}

	/* the first two bytes of the rowbuffer are reserved to hold the entire record length */
	row_pos = start + 2;
	offsets[0] = row_pos;

	tdsdump_log(TDS_DBG_FUNC, "%4s %8s %8s %8s\n", "col", "ncols", "row_pos", "max bytes");

	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		TDS_INT cpbytes = 0;
		TDSCOLUMN *bcpcol = bcpinfo->bindinfo->columns[i];
		TDS5COLINFO* const sycol = i < bcpinfo->sybase_count ? &bcpinfo->sybase_colinfo[i] : NULL;
		TDS_INT expect = column_bcp_max_size(bcpcol);

		/*
		 * Is this column of "variable" type, i.e. NULLable
		 * or naturally variable length e.g. VARCHAR
		 */
		if (sycol) {
			if (sycol->offset >= 0)
				continue;
		} else {
			if (!is_nullable_type(bcpcol->on_server.column_type) && !bcpcol->column_nullable)
				continue;
		}

		tdsdump_log(TDS_DBG_FUNC, "%4d %8d %8d %8d\n", i, ncols, row_pos, expect);

		if (TDS_FAILED(get_col_data(bcpinfo, bcpcol, offset)))
			return -1;

		/* If data was null and we have a default value, apply it */
		if (bcpcol->bcp_column_data->is_null && sycol && sycol->has_default )
		{
			tdsdump_log(TDS_DBG_INFO1, "tds5_bcp_add_variable_columns column %d applying default value\n", i + 1);
#ifdef ENABLE_EXTRA_CHECKS
			/* Sanity checks - we do not expect these to be triggered */
			if (!sycol->default_value.data || sycol->default_type != bcpcol->on_server.column_type
				|| sycol->default_value.datalen > bcpcol->on_server.column_size)
			{
				tdsdump_log(TDS_DBG_ERROR, "column %d default (%p) type %d size %d not match server type %d size %d\n",
					i + 1, (void*)sycol->default_value.data,
					sycol->default_type, (int)sycol->default_value.datalen,
					bcpcol->on_server.column_type, bcpcol->on_server.column_size);
				return -1;
			}
#endif
			cpbytes = sycol->default_value.datalen;
			if (cpbytes > expect)
			{
				tdsdump_log(TDS_DBG_ERROR, "Default value for column %d had size %d, but we expected at most %d\n", i, cpbytes, expect);
				return -1;
			}
			memcpy(&rowbuffer[row_pos], sycol->default_value.data, cpbytes);
		}
		else
		{
			/* If it's a NOT NULL column, and we have no data, throw an error.
			 * This is the behavior for Sybase, this function is only used for Sybase */
			if (!bcpcol->column_nullable && bcpcol->bcp_column_data->is_null) {
				/* No value or default value available and NULL not allowed. */
				if (null_error)
					null_error(bcpinfo, i, offset);
				return -1;
			}

			/* move the column buffer into the rowbuffer */
			if (!bcpcol->bcp_column_data->is_null) {
				if (is_blob_type(bcpcol->on_server.column_type)) {
					cpbytes = 16;
					bcpcol->column_textpos = row_pos;               /* save for data write */
				} else if (is_numeric_type(bcpcol->on_server.column_type)) {
					TDS_NUMERIC* num = (TDS_NUMERIC*)bcpcol->bcp_column_data->data;
					cpbytes = tds_numeric_bytes_per_prec[num->precision];
					memcpy(&rowbuffer[row_pos], num->array, cpbytes);
				} else {
					cpbytes = bcpcol->bcp_column_data->datalen;
					if (cpbytes > expect)
					{
						tdsdump_log(TDS_DBG_ERROR, "Value for column %d had size %d, but we expected at most %d\n", i, cpbytes, expect);
						return -1;
					}

					memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, cpbytes);
					tds5_swap_data(bcpcol, &rowbuffer[row_pos]);
				}
			}
		}

		row_pos += cpbytes;
		offsets[++ncols] = row_pos;
		tdsdump_dump_buf(TDS_DBG_NETWORK, "BCP row buffer so far", rowbuffer,  row_pos);
	}

	tdsdump_log(TDS_DBG_FUNC, "%4d %8d %8d\n", i, ncols, row_pos);

	/*
	 * The rowbuffer ends with an offset table and, optionally, an adjustment table.  
	 * The offset table has 1-byte elements that describe the locations of the start of each column in
	 * the rowbuffer.  If the largest offset is greater than 255, another table -- the adjustment table --
	 * is inserted just before the offset table.  It holds the high bytes. 
	 * 
	 * Both tables are laid out in reverse:
	 * 	#elements, offset N+1, offset N, offset N-1, ... offset 0
	 * E.g. for 2 columns you have 4 data points:
	 *	1.  How many elements (4)
	 *	2.  Start of column 3 (non-existent, "one off the end")
	 *	3.  Start of column 2
	 *	4.  Start of column 1
	 *  The length of each column is computed by subtracting its start from the its successor's start. 
	 *
	 * The algorithm below computes both tables. If the adjustment table isn't needed, the 
	 * effect is to overwrite it with the offset table.  
	 */
	while (ncols && offsets[ncols] == offsets[ncols-1])
		ncols--;	/* trailing NULL columns are not sent and are not included in the offset table */

	if (ncols && !bcpinfo->datarows_locking) {
		TDS_UCHAR *poff = rowbuffer + row_pos;
		TDS_INT pfx_top = offsets[ncols] >> 8;

		tdsdump_log(TDS_DBG_FUNC, "ncols=%u poff=%p [%u]\n", ncols, poff, offsets[ncols]);

		*poff++ = ncols + 1;
		/* this is some kind of run-length-prefix encoding */
		while (pfx_top) {
			unsigned int n_pfx = 1;

			for (i = 0; i <= ncols ; ++i)
				if ((offsets[i] >> 8) < pfx_top)
					++n_pfx;
			*poff++ = n_pfx;
			--pfx_top;
		}
   
		tdsdump_log(TDS_DBG_FUNC, "poff=%p\n", poff);

		for (i=0; i <= ncols; i++)
			*poff++ = offsets[ncols-i] & 0xFF;
		row_pos = (unsigned int)(poff - rowbuffer);
	} else {		/* Datarows-locking */
		unsigned int col;

		for (col = ncols; col-- > 0;) {
			/* The DOL BULK format has a much simpler row table -- it's just a 2-byte length for every
			 * non-fixed column (does not have the extra "offset after the end" that the basic format has) */
			rowbuffer[row_pos++] = offsets[col] % 256;
			rowbuffer[row_pos++] = offsets[col] / 256;

			tdsdump_log(TDS_DBG_FUNC, "[DOL BULK offset table] col=%u offset=%u\n", col, offsets[col]);
		}
	}

	tdsdump_log(TDS_DBG_FUNC, "%4d %8d %8d\n", i, ncols, row_pos);
	tdsdump_dump_buf(TDS_DBG_NETWORK, "BCP row buffer", rowbuffer,  row_pos);

	*pncols = ncols;

	return ncols == 0? start : row_pos;
}

/**
 * Send BCP metadata to server.
 * Only for TDS 7.0+.
 * \tds
 * \param bcpinfo BCP information
 * \return TDS_SUCCESS or TDS_FAIL.
 */
static TDSRET
tds7_bcp_send_colmetadata(TDSSOCKET *tds, TDSBCPINFO *bcpinfo)
{
	TDSCOLUMN *bcpcol;
	int i, num_cols;

	tdsdump_log(TDS_DBG_FUNC, "tds7_bcp_send_colmetadata(%p, %p)\n", tds, bcpinfo);
	assert(tds && bcpinfo);

	if (tds->out_flag != TDS_BULK || tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	/* 
	 * Deep joy! For TDS 7 we have to send a colmetadata message followed by row data
	 */
	tds_put_byte(tds, TDS7_RESULT_TOKEN);	/* 0x81 */

	num_cols = 0;
	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		bcpcol = bcpinfo->bindinfo->columns[i];
		if ((!bcpinfo->identity_insert_on && bcpcol->column_identity) || 
			bcpcol->column_timestamp ||
			bcpcol->column_computed) {
			continue;
		}
		num_cols++;
	}

	tds_put_smallint(tds, num_cols);

	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		size_t converted_len;
		const char *converted_name;

		bcpcol = bcpinfo->bindinfo->columns[i];

		/*
		 * dont send the (meta)data for timestamp columns, or
		 * identity columns (unless indentity_insert is enabled
		 */

		if ((!bcpinfo->identity_insert_on && bcpcol->column_identity) || 
			bcpcol->column_timestamp ||
			bcpcol->column_computed) {
			continue;
		}

		if (IS_TDS72_PLUS(tds->conn))
			tds_put_int(tds, bcpcol->column_usertype);
		else
			tds_put_smallint(tds, bcpcol->column_usertype);
		tds_put_smallint(tds, bcpcol->column_flags);
		TDS_PUT_BYTE(tds, bcpcol->on_server.column_type);

		assert(bcpcol->funcs);
		bcpcol->funcs->put_info(tds, bcpcol);

		/* TODO put this in put_info. It seems that parameter format is
		 * different from BCP format
		 */
		if (is_blob_type(bcpcol->on_server.column_type)) {
			converted_name = tds_convert_string(tds, tds->conn->char_convs[client2ucs2],
							    tds_dstr_cstr(&bcpinfo->tablename),
							    (int) tds_dstr_len(&bcpinfo->tablename), &converted_len);
			if (!converted_name) {
				tds_connection_close(tds->conn);
				return TDS_FAIL;
			}

			/* UTF-16 length is always size / 2 even for 4 byte letters (yes, 1 letter of length 2) */
			TDS_PUT_SMALLINT(tds, converted_len / 2);
			tds_put_n(tds, converted_name, converted_len);

			tds_convert_string_free(tds_dstr_cstr(&bcpinfo->tablename), converted_name);
		}

		converted_name = tds_convert_string(tds, tds->conn->char_convs[client2ucs2],
						    tds_dstr_cstr(&bcpcol->column_name),
						    (int) tds_dstr_len(&bcpcol->column_name), &converted_len);
		if (!converted_name) {
			tds_connection_close(tds->conn);
			return TDS_FAIL;
		}

		/* UTF-16 length is always size / 2 even for 4 byte letters (yes, 1 letter of length 2) */
		TDS_PUT_BYTE(tds, converted_len / 2);
		tds_put_n(tds, converted_name, converted_len);

		tds_convert_string_free(tds_dstr_cstr(&bcpcol->column_name), converted_name);
	}

	tds_set_state(tds, TDS_SENDING);
	return TDS_SUCCESS;
}

/**
 * Tell we finished sending BCP data to server
 * \tds
 * \param[out] rows_copied number of rows copied to server
 */
TDSRET
tds_bcp_done(TDSSOCKET *tds, int *rows_copied)
{
	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_done(%p, %p)\n", tds, rows_copied);

	if (tds->out_flag != TDS_BULK || tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_flush_packet(tds);

	tds_set_state(tds, TDS_PENDING);

	TDS_PROPAGATE(tds_process_simple_query(tds));

	if (rows_copied)
		*rows_copied = tds->rows_affected;

	return TDS_SUCCESS;
}

/**
 * Start sending BCP data to server.
 * Initialize stream to accept data.
 * \tds
 * \param bcpinfo BCP information already prepared
 */
TDSRET
tds_bcp_start(TDSSOCKET *tds, TDSBCPINFO *bcpinfo)
{
	TDSRET rc;

	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_start(%p, %p)\n", tds, bcpinfo);

	if (!IS_TDS50_PLUS(tds->conn))
		return TDS_FAIL;

	TDS_PROPAGATE(tds_submit_query(tds, bcpinfo->insert_stmt));

	/* set we want to switch to bulk state */
	tds->bulk_query = true;

	/*
	 * In TDS 5 we get the column information as a result set from the "insert bulk" command.
	 */
	if (IS_TDS50(tds->conn))
		rc = tds5_process_insert_bulk_reply(tds, bcpinfo);
	else
		rc = tds_process_simple_query(tds);
	TDS_PROPAGATE(rc);

	tds->out_flag = TDS_BULK;
	if (tds_set_state(tds, TDS_SENDING) != TDS_SENDING)
		return TDS_FAIL;

	if (IS_TDS7_PLUS(tds->conn))
		tds7_bcp_send_colmetadata(tds, bcpinfo);
	
	return TDS_SUCCESS;
}

enum {
	/* list of columns we need, 0-nnn */
	BULKCOL_colcnt,
	BULKCOL_colid,
	BULKCOL_type,
	BULKCOL_length,
	BULKCOL_status,
	BULKCOL_offset,
	BULKCOL_dflt,

	/* number of columns needed */
	BULKCOL_COUNT,

	/* bitmask to have them all */
	BULKCOL_ALL = (1 << BULKCOL_COUNT) -1,
};

static int
tds5_bulk_insert_column(const char *name)
{
#define BULKCOL(n) do {\
	if (strcmp(name, #n) == 0) \
		return BULKCOL_ ## n; \
} while(0)

	/* An impromptu hash table */
	switch (name[0]) {
	case 'c':
		BULKCOL(colcnt);
		BULKCOL(colid);
		break;
	case 'd':
		BULKCOL(dflt);
		break;
	case 't':
		BULKCOL(type);
		break;
	case 'l':
		BULKCOL(length);
		break;
	case 's':
		BULKCOL(status);
		break;
	case 'o':
		BULKCOL(offset);
		break;
	}
#undef BULKCOL
	return -1;
}

typedef struct
{
	unsigned flags;
	int pos[BULKCOL_COUNT];
	int value[BULKCOL_COUNT];
} BULKCOL_DEFINITION;

/* Detect if a ROWFMT (and subsequent ROWS) in the response is the column information
 * (i.e. avoid misinterpreting some other row received in the response stream)
 *
 * Return the data from the column response that we will need; but don't overwrite
 * that data if this is not actually the right ROWFMT.
 */
static bool is_bulkcol_formats(TDSRESULTINFO* res_info, BULKCOL_DEFINITION *out)
{
	int icol;
	BULKCOL_DEFINITION cols = { 0 };

	if (!res_info)
		return false;

	for (icol = 0; icol < res_info->num_cols; ++icol) {
		const TDSCOLUMN* col = res_info->columns[icol];
		int scol = tds5_bulk_insert_column(tds_dstr_cstr(&col->column_name));
		if (scol < 0)
			continue;
		cols.pos[scol] = icol;
		cols.flags |= 1 << scol;
	}

	if (cols.flags != BULKCOL_ALL)
		return false;

	*out = cols;
	return true;
}

static TDSRET process_bulkcol_row(TDSCONTEXT const *ctx, TDSRESULTINFO* res_info, BULKCOL_DEFINITION* pcols, TDSBCPINFO *bcpinfo)
{
	int icol;
	TDS5COLINFO* colinfo;

	if (!res_info)
		return TDS_SUCCESS;

	/* Reset column flags - we now use them to indicate if successfully converted */
	pcols->flags = 0;

	/* Extract an integer value for each column (the subset of BULKCOL
	 * columns we are interested in, are all integer types) */
	for (icol = 0; icol < BULKCOL_COUNT; ++icol) {
		const TDSCOLUMN* curcol = res_info->columns[pcols->pos[icol]];
		int ctype = tds_get_conversion_type(curcol->on_server.column_type, curcol->column_size);
		unsigned char* src = curcol->column_data;
		int srclen = curcol->column_cur_size;
		CONV_RESULT dres;

		if (tds_convert(ctx, ctype, src, srclen, SYBINT4, &dres) < 0)
			break;

		pcols->flags |= 1 << icol;
		pcols->value[icol] = dres.i;
	}

	/* Sanity checks on the column information as a whole */
	if (pcols->flags != BULKCOL_ALL ||
		pcols->value[BULKCOL_colcnt] < 1 ||
		pcols->value[BULKCOL_colcnt] > 4096 || /* limit of columns accepted */
		pcols->value[BULKCOL_colid] < 1 ||
		pcols->value[BULKCOL_colid] > pcols->value[BULKCOL_colcnt])
	{
		tdsdump_log(TDS_DBG_INFO1, "Failed sanity checks for row information");
		return TDS_FAIL;
	}

	/* Save column information for later use */
	if (bcpinfo->sybase_colinfo == NULL) {
		bcpinfo->sybase_colinfo = sybase_colinfo_alloc(pcols->value[BULKCOL_colcnt]);
		if (bcpinfo->sybase_colinfo == NULL) {
			tdsdump_log(TDS_DBG_INFO1, "Failed to allocate memory for row information");
			return TDS_FAIL;
		}
		bcpinfo->sybase_count = pcols->value[BULKCOL_colcnt];
	}

	/* bound check, colcnt could have changed from row to row */
	if (pcols->value[BULKCOL_colid] > bcpinfo->sybase_count) {
		tdsdump_log(TDS_DBG_INFO1, "colcnt changed from row to row");
		return TDS_FAIL;
	}

	colinfo = &bcpinfo->sybase_colinfo[pcols->value[BULKCOL_colid] - 1];
	colinfo->type = pcols->value[BULKCOL_type];
	colinfo->status = pcols->value[BULKCOL_status];
	colinfo->offset = pcols->value[BULKCOL_offset];
	colinfo->length = pcols->value[BULKCOL_length];
	colinfo->has_default = pcols->value[BULKCOL_dflt] && !bcpinfo->ignore_defaults;
	tdsdump_log(TDS_DBG_INFO1, "gotten row information %d type %d length %d status %d offset %d\n",
		pcols->value[BULKCOL_colid],
		colinfo->type,
		colinfo->length,
		colinfo->status,
		colinfo->offset);

	return TDS_SUCCESS;
}

static bool is_defaults_formats(TDSRESULTINFO* res_info, TDSBCPINFO* bcpinfo)
{
	int i;
	int n_defaults = 0;
	int rcolnum;

	/* TODO: figure out a more reliable way to detect this. It's not covered in the
	 * TDS 5.0 spec Version 3.4 (1999), so must have been added by ASE subsequent to that.
	 * In testing, this is the only row result we've seen after the TDS_DONE_RESULT tag.
	 *
	 * This code is based on reverse engineering the response from the test bcp_defaultdate.
	 */

	 /* Check there are the right number of defaults offered as specified.
	  * NOTE: if ignore_defaults is set, the columns will have has_default==false
	  * so this (intentionally) fails to detect and process the row.
	  */
	for (i = 0; i < bcpinfo->sybase_count; ++i)
		if (bcpinfo->sybase_colinfo[i].has_default)
			++n_defaults;

	if (n_defaults != res_info->num_cols)
		return false;

	for (i = 0, rcolnum = 0; i < bcpinfo->sybase_count && rcolnum < res_info->num_cols; ++i)
	{
		TDS5COLINFO* scol = &bcpinfo->sybase_colinfo[i];
		TDSCOLUMN* curcol = res_info->columns[rcolnum];

		if (!scol->has_default)
			continue;

		++rcolnum;

		/* Assume in correct order. In testing the res_info had column_name and
		 * table_column_name both defined to a string like "2" meaning corresponding
		 * to column 2 in the database table, we could try to validate that. */
		tdsdump_log(TDS_DBG_INFO1, "Result column \"%s\" is default value for col %d of %d\n",
			tds_dstr_cstr(&curcol->column_name), i + 1, bcpinfo->sybase_count);

		scol->default_type = curcol->column_type;

		/* Note: the actual default values will arrive in a subsequent Row,
		 * this is just the header telling us the data type for the row. */
	}

	return true;
}

static TDSRET process_defaults_row(TDSRESULTINFO* res_info, TDSBCPINFO* bcpinfo)
{
	int i, rcolnum;

	/* note: we checked in is_defaults_formats() that res_info->num_cols matches
	 * the number of columns that have "has_default" true */
	for (i = 0, rcolnum = 0; i < bcpinfo->sybase_count && rcolnum < res_info->num_cols; ++i)
	{
		TDS5COLINFO* scol = &bcpinfo->sybase_colinfo[i];
		TDSCOLUMN* curcol = res_info->columns[rcolnum];
		TDS_INT cur_size;

		if (!scol->has_default)
			continue;

		++rcolnum;

		/* The actual length of data (e.g. for VARCHAR(30) holding string len 19, this is 19) */
		cur_size = curcol->column_cur_size;

		/* Does not make sense for a default value to be NULL - we don't expect
		 * this to happen, but if it does, then treat it like no default present.
		 */
		if (!curcol->column_data || cur_size <= 0)
		{
			tdsdump_log(TDS_DBG_INFO1, "Default value had no data; skipping");
			scol->has_default = false;
			continue;
		}

		/* Testing shows that the default value can have a nullable type,
		 * even if the column is defined as NOT NULL and therefore must
		 * be packed for upload as a non-nullable type.
		 *
		 * The following defaults were observed in the "d_bcp" test:
		 *  - SYBVARCHAR (39)  various sizes
		 *  - SYBDATETIMN(111) size 4 or 8
		 *  - SYBMONEYN  (110) sizef 4 or 8
		 *  - SYBFLTN    (109) size 4 or 8
		 *  - SYBDECIMAL (106) various sizes
		 *  - SYBNUMERIC (108) various sizes
		 *  - SYBINTN    (38)  sizes 1, 2, or 4.
		 *
		 * In TDS all nullable types have the same binary format as their
		 * non-nullable equivalents (well - the other BCP code appears to make
		 * that assumption); so we can just change the type ID.
		 *
		 * See comments in tds5_process_insert_bulk_reply() regarding
		 * unpacking of DECIMAL/NUMERIC (by default the FreeTDS row unpacker
		 * actually expands these types into curcol).
		 *
		 * So, here we validate that the type of the default value either
		 * matches the server column, or is a Nullable version of the
		 * server column's type.
		 */
		if (scol->type != curcol->column_type )
		{
			TDS_SERVER_TYPE nntype = tds_get_conversion_type(curcol->column_type, cur_size);
			if (nntype != scol->type)
			{
				tdsdump_log(TDS_DBG_ERROR,
					"Default value has unexpected type %d (need type %d)\n",
					curcol->column_type, scol->type);
				continue;
			}
			scol->default_type = nntype;
		}

		/* If the server column is non-nullable
		 * we have to validate that the default is the right size for it.
		 * (nullable types have a length prefix so don't need to match length).
		 */
		if ( !is_nullable_type(scol->default_type) && cur_size != scol->length )
		{
			tdsdump_log(TDS_DBG_ERROR,
				"Default value type %d has unexpected size %d (need type %d size %d)\n",
				curcol->column_type, cur_size, scol->type, scol->length);
			/* We could "continue" to ignore this, but it's probably best to fail */
			return TDS_FAIL;
		}

		scol->default_value.data = (void *)tds_new(char, cur_size);
		if (!scol->default_value.data)
		{
			tdsdump_log(TDS_DBG_ERROR, "Failed to allocate memory for column %s default value\n",
				tds_dstr_cstr(&curcol->column_name));
			return TDS_FAIL;
		}
		memcpy(scol->default_value.data, curcol->column_data, cur_size);
		scol->default_value.datalen = cur_size;
		scol->default_value.is_null = false;

		/* Log what happened. "curcol" is the information about the default value;
		 * "scol" is the information about the server column.
		 */
		tdsdump_log(TDS_DBG_INFO1, "Default value for column %s has type %d size %d (server type %d size %d)\n",
			tds_dstr_cstr(&curcol->column_name), curcol->column_type, cur_size,	scol->type, scol->length);

		// tdsdump_dump_buf(__FILE__, __LINE__, "Default value:", curcol->column_data, curcol->column_cur_size);
	}

	return TDS_SUCCESS;
}

static TDSRET
tds5_process_insert_bulk_reply(TDSSOCKET * tds, TDSBCPINFO *bcpinfo)
{
	TDS_INT res_type;
	TDS_INT done_flags;
	TDSRET  rc;
	TDSRET  ret = TDS_SUCCESS;
	bool bulkcol_formats_found = false;
	bool bulkcol_formats_processed = false;
	bool done_seen = false;
	bool defaults_found = false;
	bool defaults_processed = false;
	BULKCOL_DEFINITION cols = { 0 };

	CHECK_TDS_EXTRA(tds);

	while ((rc = tds_process_tokens(tds, &res_type, &done_flags, TDS_RETURN_DONE|TDS_RETURN_ROWFMT|TDS_RETURN_ROW)) == TDS_SUCCESS) {
		if (res_type != TDS_ROW_RESULT)
		{
			if (bulkcol_formats_found)
				bulkcol_formats_processed = true;
		}
		switch (res_type) {
		case TDS_ROWFMT_RESULT:
			if (is_bulkcol_formats(tds->current_results, &cols))
				bulkcol_formats_found = true;
			/* In testing, Defaults only come after the TDS_DONE token */
			else if (done_seen && is_defaults_formats(tds->current_results, bcpinfo))
			{
				extern TDSCOLUMNFUNCS tds_generic_funcs;
				// We now have to inform FreeTDS to NOT perform any processing on the raw
				// values of defaults. We need to just save the value exactly as received
				// since that's what the upload expects.
				// If we don't do this, then for example a NUMERIC()
				// will be unpacked to a 35-byte structure in the row results.
				//
				// The tds5_send_record() function does not currently use the "put_data"
				// member of the column functions , it just rolls its own packing. It
				// might be possible to look into using put_data for bcp record packing,
				// then we wouldn't have to do this hack here.
				for (int i = 0; i < tds->current_results->num_cols; ++i)
					tds->current_results->columns[i]->funcs = &tds_generic_funcs;
				// Don't need to reset it as there is no other row data after the Defaults
				// (they're part of the End block)
				defaults_found = true;
			}
			break;

		case TDS_ROW_RESULT:
			/* Defaults values will follow defaults formats */
			if (defaults_found && !defaults_processed)
			{
				rc = process_defaults_row(tds->current_results, bcpinfo);

				/* The defaults all arrive as a single row, one column per default value */
				defaults_processed = true;
			}

			/* One row per column format definition */
			if (bulkcol_formats_found && !bulkcol_formats_processed)
				rc = process_bulkcol_row(tds_get_ctx(tds), tds->current_results, &cols, bcpinfo);

			break;

		case TDS_DONE_RESULT:
		case TDS_DONEPROC_RESULT:
		case TDS_DONEINPROC_RESULT:
			done_seen = true;
			if ((done_flags & TDS_DONE_ERROR) != 0)
				ret = TDS_FAIL;
		default:
			break;
		}
	}
	if (TDS_FAILED(rc))
		ret = rc;

	return ret;
}

/**
 * Free row data allocated in the result set.
 */
static void 
tds_bcp_row_free(TDSRESULTINFO* result, unsigned char *row TDS_UNUSED)
{
	result->row_size = 0;
	TDS_ZERO_FREE(result->current_row);
}

/**
 * Start bulk copy to server
 * \tds
 * \param bcpinfo BCP information already prepared
 */
TDSRET
tds_bcp_start_copy_in(TDSSOCKET *tds, TDSBCPINFO *bcpinfo)
{
	TDSCOLUMN *bcpcol;
	int i;
	TDS_INT fixed_col_len_tot     = 0;
	TDS_INT variable_col_len_tot  = 0;
	TDS_INT column_bcp_data_size  = 0;
	TDS_INT bcp_record_size       = 0;
	TDSRET rc;
	TDS_INT var_cols;
	
	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_start_copy_in(%p, %p)\n", tds, bcpinfo);

	TDS_PROPAGATE(tds_bcp_start_insert_stmt(tds, bcpinfo));

	rc = tds_bcp_start(tds, bcpinfo);
	if (TDS_FAILED(rc)) {
		/* TODO, in CTLib was _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 5, 1, 140, ""); */
		return rc;
	}

	/*
	 * TDS5 BCP is coded to pack all columns into an internal buffer and
	 * then send the buffer (as opposed to TDS7 BCP which just calls wire
	 * put functions for each column). So we have to work out the maximum
	 * possible required buffer size.
	 */
	var_cols = 0;

	if (IS_TDS50(tds->conn)) {
		for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
	
			bcpcol = bcpinfo->bindinfo->columns[i];
			column_bcp_data_size = column_bcp_max_size(bcpcol);

			/*
			 * now add that size into either fixed or variable
			 * column totals...
			 */
			if (is_nullable_type(bcpcol->on_server.column_type) || bcpcol->column_nullable) {
				var_cols++;
				variable_col_len_tot += column_bcp_data_size;
			} else {
				fixed_col_len_tot += column_bcp_data_size;
			}
		}

		/* this formula taken from sybase manual...
		 * (with extra 4 bytes possibly for datarows locked) */

		bcp_record_size =  	4 +
							(bcpinfo->datarows_locking ? 4 : 0) +
							fixed_col_len_tot +
							variable_col_len_tot +
							( (int)(variable_col_len_tot / 256 ) + 1 ) +
							(var_cols + 1) +
							2;

		tdsdump_log(TDS_DBG_FUNC, "current_record_size = %d\n", bcpinfo->bindinfo->row_size);
		tdsdump_log(TDS_DBG_FUNC, "bcp_record_size     = %d\n", bcp_record_size);

		if (bcp_record_size > bcpinfo->bindinfo->row_size) {
			/* Expand our internal row buffer to be able to hold all of BCP-packed row
			 * (We re-use the internal row buffer for BCP packing, since we don't need
			 *  to read it again after the BCP upload)
			 */
			if (!TDS_RESIZE(bcpinfo->bindinfo->current_row, bcp_record_size)) {
				tdsdump_log(TDS_DBG_WARN, "could not realloc current_row\n");
				return TDS_FAIL;
			}
			bcpinfo->bindinfo->row_free = tds_bcp_row_free;
			bcpinfo->bindinfo->row_size = bcp_record_size;
		}
	}

	return TDS_SUCCESS;
}

/** input stream to read a file */
typedef struct tds_file_stream {
	/** common fields, must be the first field */
	TDSINSTREAM stream;
	/** file to read from */
	FILE *f;

	/** terminator */
	const char *terminator;
	/** terminator length in bytes */
	size_t term_len;

	/** buffer for store bytes readed that could be the terminator */
	char *left;
	size_t left_pos;
} TDSFILESTREAM;

/** \cond HIDDEN_SYMBOLS */
#if defined(_WIN32) && defined(HAVE__LOCK_FILE) && defined(HAVE__UNLOCK_FILE)
#define TDS_HAVE_STDIO_LOCKED 1
#define flockfile(s) _lock_file(s)
#define funlockfile(s) _unlock_file(s)
#define getc_unlocked(s) _getc_nolock(s)
#define feof_unlocked(s) _feof_nolock(s)
#endif

#ifndef TDS_HAVE_STDIO_LOCKED
#undef getc_unlocked
#undef feof_unlocked
#undef flockfile
#undef funlockfile
#define getc_unlocked(s) getc(s)
#define feof_unlocked(s) feof(s)
#define flockfile(s) do { } while(0)
#define funlockfile(s) do { } while(0)
#endif
/** \endcond */

/**
 * Reads a chunk of data from file stream checking for terminator
 * \param stream file stream
 * \param ptr buffer where to read data
 * \param len length of buffer
 */
static int
tds_file_stream_read(TDSINSTREAM *stream, void *ptr, size_t len)
{
	TDSFILESTREAM *s = (TDSFILESTREAM *) stream;
	int c;
	char *p = (char *) ptr;

	while (len) {
		if (memcmp(s->left, s->terminator - s->left_pos, s->term_len) == 0)
			return p - (char *) ptr;

		c = getc_unlocked(s->f);
		if (c == EOF)
			return -1;

		*p++ = s->left[s->left_pos];
		--len;

		s->left[s->left_pos++] = c;
		s->left_pos %= s->term_len;
	}
	return p - (char *) ptr;
}

/**
 * Read a data file, passing the data through iconv().
 * \retval TDS_SUCCESS  success
 * \retval TDS_FAIL     error reading the column
 * \retval TDS_NO_MORE_RESULTS end of file detected
 */
TDSRET
tds_bcp_fread(TDSSOCKET * tds, TDSICONV * char_conv, FILE * stream, const char *terminator, size_t term_len, char **outbuf, size_t * outbytes)
{
	TDSRET res;
	TDSFILESTREAM r;
	TDSDYNAMICSTREAM w;
	size_t readed;

	/* prepare streams */
	r.stream.read = tds_file_stream_read;
	r.f = stream;
	r.term_len = term_len;
	r.left = tds_new0(char, term_len*3);
	r.left_pos = 0;
	if (!r.left) return TDS_FAIL;

	/* copy terminator twice, let terminator points to second copy */
	memcpy(r.left + term_len, terminator, term_len);
	memcpy(r.left + term_len*2u, terminator, term_len);
	r.terminator = r.left + term_len*2u;

	/* read initial buffer to test with terminator */
	readed = fread(r.left, 1, term_len, stream);
	if (readed != term_len) {
		free(r.left);
		if (readed == 0 && feof(stream))
			return TDS_NO_MORE_RESULTS;
		return TDS_FAIL;
	}

	res = tds_dynamic_stream_init(&w, (void**) outbuf, 0);
	if (TDS_FAILED(res)) {
		free(r.left);
		return res;
	}

	/* convert/copy from input stream to output one */
	flockfile(stream);
	if (char_conv == NULL)
		res = tds_copy_stream(&r.stream, &w.stream);
	else
		res = tds_convert_stream(tds, char_conv, to_server, &r.stream, &w.stream);
	funlockfile(stream);
	free(r.left);

	TDS_PROPAGATE(res);

	*outbytes = w.size;

	/* terminate buffer */
	if (!w.stream.buf_len)
		return TDS_FAIL;

	((char *) w.stream.buffer)[0] = 0;
	w.stream.write(&w.stream, 1);

	return res;
}

/**
 * Start writing writetext request.
 * This request start a bulk session.
 * \tds
 * \param objname table name
 * \param textptr TEXTPTR (see sql documentation)
 * \param timestamp data timestamp
 * \param with_log is log is enabled during insert
 * \param size bytes to be inserted
 */
TDSRET
tds_writetext_start(TDSSOCKET *tds, const char *objname, const char *textptr, const char *timestamp, int with_log, TDS_UINT size)
{
	TDSRET rc;

	/* TODO mssql does not like timestamp */
	rc = tds_submit_queryf(tds,
			      "writetext bulk %s 0x%s timestamp = 0x%s%s",
			      objname, textptr, timestamp, with_log ? " with log" : "");
	TDS_PROPAGATE(rc);

	/* set we want to switch to bulk state */
	tds->bulk_query = true;

	/* read the end token */
	TDS_PROPAGATE(tds_process_simple_query(tds));

	tds->out_flag = TDS_BULK;
	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_put_int(tds, size);

	tds_set_state(tds, TDS_SENDING);
	return TDS_SUCCESS;
}

/**
 * Send some data in the writetext request started by tds_writetext_start.
 * You should write in total (with multiple calls to this function) all
 * bytes declared calling tds_writetext_start.
 * \tds
 * \param text data to write
 * \param size data size in bytes
 */
TDSRET
tds_writetext_continue(TDSSOCKET *tds, const TDS_UCHAR *text, TDS_UINT size)
{
	if (tds->out_flag != TDS_BULK || tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	/* TODO check size left */
	tds_put_n(tds, text, size);

	tds_set_state(tds, TDS_SENDING);
	return TDS_SUCCESS;
}

/**
 * Finish sending writetext data.
 * \tds
 */
TDSRET
tds_writetext_end(TDSSOCKET *tds)
{
	if (tds->out_flag != TDS_BULK || tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_flush_packet(tds);
	tds_set_state(tds, TDS_PENDING);
	return TDS_SUCCESS;
}
