/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <assert.h>

#include "tds.h"
#include "tdsconvert.h"
#include "tdsiconv.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: token.c,v 1.224 2003-11-13 19:15:47 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version,
	no_unused_var_warn
};

static int tds_process_msg(TDSSOCKET * tds, int marker);
static int tds_process_compute_result(TDSSOCKET * tds);
static int tds_process_compute_names(TDSSOCKET * tds);
static int tds7_process_compute_result(TDSSOCKET * tds);
static int tds_process_result(TDSSOCKET * tds);
static int tds_process_col_name(TDSSOCKET * tds);
static int tds_process_col_fmt(TDSSOCKET * tds);
static int tds_process_colinfo(TDSSOCKET * tds);
static int tds_process_compute(TDSSOCKET * tds, TDS_INT * computeid);
static int tds_process_cursor_tokens(TDSSOCKET * tds);
static int tds_process_row(TDSSOCKET * tds);
static int tds_process_param_result(TDSSOCKET * tds, TDSPARAMINFO ** info);
static int tds7_process_result(TDSSOCKET * tds);
static TDSDYNAMIC *tds_process_dynamic(TDSSOCKET * tds);
static int tds_process_auth(TDSSOCKET * tds);
static int tds_get_data(TDSSOCKET * tds, TDSCOLINFO * curcol, unsigned char *current_row, int i);
static int tds_get_data_info(TDSSOCKET * tds, TDSCOLINFO * curcol);
static int tds_process_env_chg(TDSSOCKET * tds);
static const char *_tds_token_name(unsigned char marker);
static int tds_process_param_result_tokens(TDSSOCKET * tds);
static int tds_process_params_result_token(TDSSOCKET * tds);
static int tds_process_dyn_result(TDSSOCKET * tds);
static int tds5_get_varint_size(int datatype);
static int tds5_process_result(TDSSOCKET * tds);
static int tds5_process_dyn_result2(TDSSOCKET * tds);
static void adjust_character_column_size(const TDSSOCKET * tds, TDSCOLINFO * curcol);
static int determine_adjusted_size(const TDSICONVINFO * iconv_info, int size);
static int tds_process_default_tokens(TDSSOCKET * tds, int marker);
static TDS_INT tds_process_end(TDSSOCKET * tds, int marker, int *flags_parm);
static int _tds_process_row_tokens(TDSSOCKET * tds, TDS_INT * rowtype, TDS_INT * computeid, TDS_INT read_end_token);
static const char *tds_pr_op(int op);


/**
 * \ingroup libtds
 * \defgroup token Results processing
 * Handle tokens in packets. Many PDU (packets data unit) contain tokens.
 * (like result description, rows, data, errors and many other).
 */


/** \addtogroup token
 *  \@{ 
 */

/**
 * tds_process_default_tokens() is a catch all function that is called to
 * process tokens not known to other tds_process_* routines
 */
static int
tds_process_default_tokens(TDSSOCKET * tds, int marker)
{
	int tok_size;
	int done_flags;

	tdsdump_log(TDS_DBG_FUNC, "%L tds_process_default_tokens() marker is %x(%s)\n", marker, _tds_token_name(marker));

	if (IS_TDSDEAD(tds)) {
		tdsdump_log(TDS_DBG_FUNC, "%L leaving tds_process_default_tokens() connection dead\n");
		tds->state = TDS_DEAD;
		return TDS_FAIL;
	}

	switch (marker) {
	case TDS_AUTH_TOKEN:
		return tds_process_auth(tds);
		break;
	case TDS_ENVCHANGE_TOKEN:
		return tds_process_env_chg(tds);
		break;
	case TDS_DONE_TOKEN:
	case TDS_DONEPROC_TOKEN:
	case TDS_DONEINPROC_TOKEN:
		return tds_process_end(tds, marker, &done_flags);
		break;
	case TDS_PROCID_TOKEN:
		tds_get_n(tds, NULL, 8);
		break;
	case TDS_RETURNSTATUS_TOKEN:
		tds->has_status = 1;
		tds->ret_status = tds_get_int(tds);
		tdsdump_log(TDS_DBG_FUNC, "%L tds_process_default_tokens: return status is %d\n", tds->ret_status);
		break;
	case TDS_ERROR_TOKEN:
	case TDS_INFO_TOKEN:
	case TDS_EED_TOKEN:
		return tds_process_msg(tds, marker);
		break;
	case TDS_CAPABILITY_TOKEN:
		/* TODO split two part of capability and use it */
		tok_size = tds_get_smallint(tds);
		/* vicm */
		/* Sybase 11.0 servers return the wrong length in the capability packet, causing use to read
		 * past the done packet.
		 */
		if (!TDS_IS_MSSQL(tds) && tds->product_version < TDS_SYB_VER(12, 0, 0)) {
			unsigned char type, size, *p, *pend;

			p = tds->capabilities;
			pend = tds->capabilities + TDS_MAX_CAPABILITY;

			do {
				type = tds_get_byte(tds);
				size = tds_get_byte(tds);
				if ((p + 2) > pend)
					break;
				*p++ = type;
				*p++ = size;
				if ((p + size) > pend)
					break;
				if (tds_get_n(tds, p, size) == NULL)
					return TDS_FAIL;
			} while (type != 2);
		} else {
			if (tds_get_n(tds, tds->capabilities, tok_size > TDS_MAX_CAPABILITY ? TDS_MAX_CAPABILITY : tok_size) ==
			    NULL)
				return TDS_FAIL;
		}
		break;
		/* PARAM_TOKEN can be returned inserting text in db, to return new timestamp */
	case TDS_PARAM_TOKEN:
		tds_unget_byte(tds);
		return tds_process_param_result_tokens(tds);
		break;
	case TDS7_RESULT_TOKEN:
		return tds7_process_result(tds);
		break;
	case TDS_OPTIONCMD_TOKEN:
		tdsdump_log(TDS_DBG_FUNC, "%L option command token encountered\n");
		break;
	case TDS_RESULT_TOKEN:
		return tds_process_result(tds);
		break;
	case TDS_ROWFMT2_TOKEN:
		return tds5_process_result(tds);
		break;
	case TDS_COLNAME_TOKEN:
		return tds_process_col_name(tds);
		break;
	case TDS_COLFMT_TOKEN:
		return tds_process_col_fmt(tds);
		break;
	case TDS_ROW_TOKEN:
		return tds_process_row(tds);
		break;
	case TDS5_PARAMFMT_TOKEN:
		/* store discarded parameters in param_info, not in old dynamic */
		tds->cur_dyn = NULL;
		return tds_process_dyn_result(tds);
		break;
	case TDS5_PARAMFMT2_TOKEN:
		tds->cur_dyn = NULL;
		return tds5_process_dyn_result2(tds);
		break;
	case TDS5_PARAMS_TOKEN:
		/* save params */
		return tds_process_params_result_token(tds);
		break;
	case TDS_CURINFO_TOKEN:
		return tds_process_cursor_tokens(tds);
		break;
	case TDS5_DYNAMIC_TOKEN:
	case TDS_LOGINACK_TOKEN:
	case TDS_ORDERBY_TOKEN:
	case TDS_CONTROL_TOKEN:
	case TDS_TABNAME_TOKEN:	/* used for FOR BROWSE query */
		tdsdump_log(TDS_DBG_WARN, "eating token %d\n", marker);
		tds_get_n(tds, NULL, tds_get_smallint(tds));
		break;
	case TDS_COLINFO_TOKEN:
		return tds_process_colinfo(tds);
		break;
	case TDS_ORDERBY2_TOKEN:
		tdsdump_log(TDS_DBG_WARN, "eating token %d\n", marker);
		tds_get_n(tds, NULL, tds_get_int(tds));
		break;
	default:
		if (IS_TDSDEAD(tds))
			tds->state = TDS_DEAD;
		/* TODO perhaps is best to close this connection... */
		tdsdump_log(TDS_DBG_ERROR, "Unknown marker: %d(%x)!!\n", marker, (unsigned char) marker);
		return TDS_FAIL;
	}
	return TDS_SUCCEED;
}

static int
tds_set_spid(TDSSOCKET * tds)
{
	TDS_INT result_type;
	TDS_INT done_flags;
	TDS_INT row_type;
	TDS_INT compute_id;
	TDS_INT rc;
	TDSCOLINFO *curcol;

	if (tds_submit_query(tds, "select @@spid") != TDS_SUCCEED) {
		return TDS_FAIL;
	}

	while ((rc = tds_process_result_tokens(tds, &result_type, &done_flags)) == TDS_SUCCEED) {

		switch (result_type) {

			case TDS_ROWFMT_RESULT:
				if (tds->res_info->num_cols != 1) 
					return TDS_FAIL;
				break;

			case TDS_ROW_RESULT:
				while ((rc = tds_process_row_tokens(tds, &row_type, &compute_id)) == TDS_SUCCEED);

				if (rc != TDS_NO_MORE_ROWS)
					return TDS_FAIL;

				curcol = tds->res_info->columns[0];
				if (curcol->column_type == SYBINT2 || (curcol->column_type == SYBINTN && curcol->column_size == 2)) {
					tds->spid = *((TDS_USMALLINT *) (tds->res_info->current_row + curcol->column_offset));
				} else if (curcol->column_type == SYBINT4 || (curcol->column_type == SYBINTN && curcol->column_size == 4)) {
					tds->spid = *((TDS_UINT *) (tds->res_info->current_row + curcol->column_offset));
				} else
					return TDS_FAIL;
				break;

			case TDS_DONE_RESULT:
				if ((done_flags & TDS_DONE_ERROR) != 0)
					return TDS_FAIL;
				break;

			default:
				break;
		}
	}
	if (rc != TDS_NO_MORE_RESULTS) 
		return TDS_FAIL;

	return TDS_SUCCEED;
}

/**
 * tds_process_login_tokens() is called after sending the login packet 
 * to the server.  It returns the success or failure of the login 
 * dependent on the protocol version. 4.2 sends an ACK token only when
 * successful, TDS 5.0 sends it always with a success byte within
 */
int
tds_process_login_tokens(TDSSOCKET * tds)
{
	int succeed = TDS_FAIL;
	int marker;
	int len;
	int memrc = 0;
	unsigned char major_ver, minor_ver;
	unsigned char ack;
	TDS_UINT product_version;

	tdsdump_log(TDS_DBG_FUNC, "%L tds_process_login_tokens()\n");
	/* get_incoming(tds->s); */
	do {
		marker = tds_get_byte(tds);
		tdsdump_log(TDS_DBG_FUNC, "%L looking for login token, got  %x(%s)\n", marker, _tds_token_name(marker));

		switch (marker) {
		case TDS_AUTH_TOKEN:
			tds_process_auth(tds);
			break;
		case TDS_LOGINACK_TOKEN:
			/* TODO function */
			len = tds_get_smallint(tds);
			ack = tds_get_byte(tds);
			major_ver = tds_get_byte(tds);
			minor_ver = tds_get_byte(tds);
			tds_get_n(tds, NULL, 2);
			/* ignore product name length, see below */
			tds_get_byte(tds);
			product_version = 0;
			/* get server product name */
			/* compute length from packet, some version seem to fill this information wrongly */
			len -= 10;
			if (tds->product_name)
				free(tds->product_name);
			if (major_ver >= 7) {
				product_version = 0x80000000u;
				memrc += tds_alloc_get_string(tds, &tds->product_name, len / 2);
			} else if (major_ver >= 5) {
				memrc += tds_alloc_get_string(tds, &tds->product_name, len);
			} else {
				memrc += tds_alloc_get_string(tds, &tds->product_name, len);
				if (tds->product_name != NULL && strstr(tds->product_name, "Microsoft"))
					product_version = 0x80000000u;
			}
			product_version |= ((TDS_UINT) tds_get_byte(tds)) << 24;
			product_version |= ((TDS_UINT) tds_get_byte(tds)) << 16;
			product_version |= ((TDS_UINT) tds_get_byte(tds)) << 8;
			product_version |= tds_get_byte(tds);
			/* MSSQL 6.5 and 7.0 seem to return strange values for this 
			 * using TDS 4.2, something like 5F 06 32 FF for 6.50 */
			if (major_ver == 4 && minor_ver == 2 && (product_version & 0xff0000ffu) == 0x5f0000ffu)
				product_version = ((product_version & 0xffff00u) | 0x800000u) << 8;
			tds->product_version = product_version;
#ifdef WORDS_BIGENDIAN
			/* TODO do a best check */
/*
				
				if (major_ver==7) {
					tds->broken_dates=1;
				}
*/
#endif
			/* TDS 5.0 reports 5 on success 6 on failure
			 * TDS 4.2 reports 1 on success and is not
			 * present on failure */
			if (ack == 5 || ack == 1)
				succeed = TDS_SUCCEED;
			break;
		default:
			if (tds_process_default_tokens(tds, marker) == TDS_FAIL)
				return TDS_FAIL;
			break;
		}
	} while (marker != TDS_DONE_TOKEN);
	/* TODO why ?? */
	tds->spid = tds->rows_affected;
	if (tds->spid == 0) {
		if (tds_set_spid(tds) != TDS_SUCCEED) {
			tdsdump_log(TDS_DBG_ERROR, "%L tds_set_spid() failed\n");
			succeed = TDS_FAIL;
		}
	}
	tdsdump_log(TDS_DBG_FUNC, "%L leaving tds_process_login_tokens() returning %d\n", succeed);
	if (memrc != 0)
		succeed = TDS_FAIL;
	return succeed;
}

static int
tds_process_auth(TDSSOCKET * tds)
{
	int pdu_size;
	unsigned char nonce[8];

/* char domain[30]; */
	int where = 0;

#if ENABLE_EXTRA_CHECKS
	if (!IS_TDS7_PLUS(tds))
		tdsdump_log(TDS_DBG_ERROR, "Called auth on TDS version < 7\n");
#endif

	pdu_size = tds_get_smallint(tds);
	tdsdump_log(TDS_DBG_INFO1, "TDS_AUTH_TOKEN PDU size %d\n", pdu_size);

	/* TODO check first 2 values */
	tds_get_n(tds, NULL, 8);	/* NTLMSSP\0 */
	where += 8;
	tds_get_int(tds);	/* sequence -> 2 */
	where += 4;
	tds_get_n(tds, NULL, 4);	/* domain len (2 time) */
	where += 4;
	tds_get_int(tds);	/* domain offset */
	where += 4;
	/* TODO use them */
	tds_get_n(tds, NULL, 4);	/* flags */
	where += 4;
	tds_get_n(tds, nonce, 8);
	where += 8;
	tdsdump_log(TDS_DBG_INFO1, "TDS_AUTH_TOKEN nonce\n");
	tdsdump_dump_buf(nonce, 8);
	tds_get_n(tds, NULL, 8);	/* ?? */
	where += 8;

	/*
	 * tds_get_string(tds, domain, domain_len); 
	 * tdsdump_log(TDS_DBG_INFO1, "TDS_AUTH_TOKEN domain %s\n", domain);
	 * where += strlen(domain);
	 */

	if (pdu_size < where)
		return TDS_FAIL;
	tds_get_n(tds, NULL, pdu_size - where);
	tdsdump_log(TDS_DBG_INFO1, "%L Draining %d bytes\n", pdu_size - where);

	tds7_send_auth(tds, nonce);

	return TDS_SUCCEED;
}

/**
 * process TDS result-type message streams.
 * tds_process_result_tokens() is called after submitting a query with
 * tds_submit_query() and is responsible for calling the routines to
 * populate tds->res_info if appropriate (some query have no result sets)
 * @param tds A pointer to the TDSSOCKET structure managing a client/server operation.
 * @param result_type A pointer to an integer variable which 
 *        tds_process_result_tokens sets to indicate the current type of result.
 *  @par
 *  <b>Values that indicate command status</b>
 *  <table>
 *   <tr><td>TDS_DONE_RESULT</td><td>The results of a command have been completely processed. This command return no rows.</td></tr>
 *   <tr><td>TDS_DONEPROC_RESULT</td><td>The results of a  command have been completely processed. This command return rows.</td></tr>
 *   <tr><td>TDS_DONEINPROC_RESULT</td><td>The results of a  command have been completely processed. This command return rows.</td></tr>
 *  </table>
 *  <b>Values that indicate results information is available</b>
 *  <table><tr>
 *    <td>TDS_ROWFMT_RESULT</td><td>Regular Data format information</td>
 *    <td>tds->res_info now contains the result details ; tds->curr_resinfo now points to that data</td>
 *   </tr><tr>
 *    <td>TDS_COMPUTEFMT_ RESULT</td><td>Compute data format information</td>
 *    <td>tds->comp_info now contains the result data; tds->curr_resinfo now points to that data</td>
 *   </tr><tr>
 *    <td>TDS_DESCRIBE_RESULT</td><td></td>
 *    <td></td>
 *  </tr></table>
 *  <b>Values that indicate data is available</b>
 *  <table><tr>
 *   <td><b>Value</b></td><td><b>Meaning</b></td><td><b>Information returned</b></td>
 *   </tr><tr>
 *    <td>TDS_ROW_RESULT</td><td>Regular row results</td>
 *    <td>1 or more rows of regular data can now be retrieved</td>
 *   </tr><tr>
 *    <td>TDS_COMPUTE_RESULT</td><td>Compute row results</td>
 *    <td>A single row of compute data can now be retrieved</td>
 *   </tr><tr>
 *    <td>TDS_PARAM_RESULT</td><td>Return parameter results</td>
 *    <td>param_info or cur_dyn->params contain returned parameters</td>
 *   </tr><tr>
 *    <td>TDS_STATUS_RESULT</td><td>Stored procedure status results</td>
 *    <td>tds->ret_status contain the returned code</td>
 *  </tr></table>
 * @todo Complete TDS_DESCRIBE_RESULT description
 * @retval TDS_SUCCEED if a result set is available for processing.
 * @retval TDS_NO_MORE_RESULTS if all results have been completely processed.
 * @par Examples
 * The following code is cut from ct_results(), the ct-library function
 * @include token1.c
 */
int
tds_process_result_tokens(TDSSOCKET * tds, TDS_INT * result_type, int *done_flags)
{
	int marker;
	TDSPARAMINFO *pinfo = (TDSPARAMINFO *)NULL;
	TDSCOLINFO   *curcol;
	int saved_rows_affected = TDS_NO_COUNT;
	int saved_return_status = 0;
	int rc;

	if (tds->state == TDS_IDLE) {
		tdsdump_log(TDS_DBG_FUNC, "%L tds_process_result_tokens() state is COMPLETED\n");
		*result_type = TDS_DONE_RESULT;
		return TDS_NO_MORE_RESULTS;
	}

	tds->curr_resinfo = NULL;
	for (;;) {

		marker = tds_get_byte(tds);
		tdsdump_log(TDS_DBG_INFO1, "%L processing result tokens.  marker is  %x(%s)\n", marker, _tds_token_name(marker));

		switch (marker) {
		case TDS7_RESULT_TOKEN:
			rc = tds7_process_result(tds);

			/* If we're processing the results of a cursor fetch */
			/* from sql server we don't want to pass back the    */
			/* TDS_ROWFMT_RESULT to the calling API              */

			if (tds->internal_sp_called == TDS_SP_CURSORFETCH) {
				marker = tds_get_byte(tds);
				if (marker != TDS_TABNAME_TOKEN) {
					tds_unget_byte(tds);
				} else {
					tds_process_default_tokens(tds, marker);
					marker = tds_get_byte(tds);
					if (marker != TDS_COLINFO_TOKEN) {
						tds_unget_byte(tds);
					} else {
						tds_process_colinfo(tds);
					}
				}
			} else {
				*result_type = TDS_ROWFMT_RESULT;
				/* handle browse information (if presents) */
				/* TODO copied from below, function or put in results process */
				marker = tds_get_byte(tds);
				if (marker != TDS_TABNAME_TOKEN) {
					tds_unget_byte(tds);
					return TDS_SUCCEED;
				}
				tds_process_default_tokens(tds, marker);
				marker = tds_get_byte(tds);
				if (marker != TDS_COLINFO_TOKEN) {
					tds_unget_byte(tds);
					return TDS_SUCCEED;
				}
				if (rc == TDS_FAIL)
					return TDS_FAIL;
				else {
					tds_process_colinfo(tds);
					return TDS_SUCCEED;
				}
			}
			break;
		case TDS_RESULT_TOKEN:
			*result_type = TDS_ROWFMT_RESULT;
			return tds_process_result(tds);
			break;
		case TDS_ROWFMT2_TOKEN:
			*result_type = TDS_ROWFMT_RESULT;
			return tds5_process_result(tds);
			break;
		case TDS_COLNAME_TOKEN:
			tds_process_col_name(tds);
			break;
		case TDS_COLFMT_TOKEN:
			rc = tds_process_col_fmt(tds);
			*result_type = TDS_ROWFMT_RESULT;
			/* handle browse information (if presents) */
			marker = tds_get_byte(tds);
			if (marker != TDS_TABNAME_TOKEN) {
				tds_unget_byte(tds);
				return rc;
			}
			tds_process_default_tokens(tds, marker);
			marker = tds_get_byte(tds);
			if (marker != TDS_COLINFO_TOKEN) {
				tds_unget_byte(tds);
				return rc;
			}
			if (rc == TDS_FAIL)
				return TDS_FAIL;
			else {
				tds_process_colinfo(tds);
				return TDS_SUCCEED;
			}
			break;
		case TDS_PARAM_TOKEN:
			tds_unget_byte(tds);
			if (tds->internal_sp_called) {
				tdsdump_log(TDS_DBG_FUNC, "%L processing parameters for sp %d\n", tds->internal_sp_called);
				while ((marker = tds_get_byte(tds)) == TDS_PARAM_TOKEN) {
					tdsdump_log(TDS_DBG_INFO1, "%L calling tds_process_param_result\n");
					tds_process_param_result(tds, &pinfo);
				}
				tds_unget_byte(tds);
				tdsdump_log(TDS_DBG_FUNC, "%L no of hidden return parameters %d\n", pinfo->num_cols);
				if(tds->internal_sp_called == TDS_SP_CURSOROPEN) {
					curcol = pinfo->columns[0];
					tds->cursor->cursor_id = *(TDS_INT *) &(pinfo->current_row[curcol->column_offset]);
				}
				if(tds->internal_sp_called == TDS_SP_PREPARE) {
					curcol = pinfo->columns[0];
					if (tds->cur_dyn && tds->cur_dyn->num_id == 0 && !tds_get_null(pinfo->current_row, 0)) {
						tds->cur_dyn->num_id = *(TDS_INT *) &(pinfo->current_row[curcol->column_offset]);
					}
				}
				tds_free_param_results(pinfo);
			} else {
				tds_process_param_result_tokens(tds);
				*result_type = TDS_PARAM_RESULT;
				return TDS_SUCCEED;
			}
			break;
		case TDS_COMPUTE_NAMES_TOKEN:
			return tds_process_compute_names(tds);
			break;
		case TDS_COMPUTE_RESULT_TOKEN:
			*result_type = TDS_COMPUTEFMT_RESULT;
			return tds_process_compute_result(tds);
			break;
		case TDS7_COMPUTE_RESULT_TOKEN:
			tds7_process_compute_result(tds);
			*result_type = TDS_COMPUTEFMT_RESULT;
			return TDS_SUCCEED;
			break;
		case TDS_ROW_TOKEN:
			/* overstepped the mark... */
			*result_type = TDS_ROW_RESULT;
			tds->res_info->rows_exist = 1;
			tds->curr_resinfo = tds->res_info;
			tds_unget_byte(tds);
			return TDS_SUCCEED;
			break;
		case TDS_CMP_ROW_TOKEN:
			*result_type = TDS_COMPUTE_RESULT;
			tds->res_info->rows_exist = 1;
			tds_unget_byte(tds);
			return TDS_SUCCEED;
			break;
		case TDS_RETURNSTATUS_TOKEN:
			if (tds->internal_sp_called) {
				saved_return_status = tds_get_int(tds);
			} else {
				tds->has_status = 1;
				tds->ret_status = tds_get_int(tds);
				*result_type = TDS_STATUS_RESULT;
				return TDS_SUCCEED;
			}
			break;
		case TDS5_DYNAMIC_TOKEN:
			/* process acknowledge dynamic */
			tds->cur_dyn = tds_process_dynamic(tds);
			break;
		case TDS5_PARAMFMT_TOKEN:
			tds_process_dyn_result(tds);
			*result_type = TDS_DESCRIBE_RESULT;
			return TDS_SUCCEED;
			break;
		case TDS5_PARAMFMT2_TOKEN:
			tds5_process_dyn_result2(tds);
			*result_type = TDS_DESCRIBE_RESULT;
			return TDS_SUCCEED;
			break;
		case TDS5_PARAMS_TOKEN:
			tds_process_params_result_token(tds);
			*result_type = TDS_PARAM_RESULT;
			return TDS_SUCCEED;
			break;
		case TDS_CURINFO_TOKEN:
			tds_process_cursor_tokens(tds);
			break;
		case TDS_DONE_TOKEN:
			tds_process_end(tds, marker, done_flags);
			*result_type = TDS_DONE_RESULT;
			return TDS_SUCCEED;
		case TDS_DONEPROC_TOKEN:
			tds_process_end(tds, marker, done_flags);
			if (tds->internal_sp_called) {
				*result_type       = TDS_DONE_RESULT;
				tds->rows_affected = saved_rows_affected;
			} else {
				*result_type = TDS_DONEPROC_RESULT;
			}
			return TDS_SUCCEED;
		case TDS_DONEINPROC_TOKEN:
			/* FIXME should we free results ?? */
			tds_process_end(tds, marker, done_flags);
			if (tds->internal_sp_called) {
				if (tds->rows_affected != TDS_NO_COUNT) {
					saved_rows_affected = tds->rows_affected;
				} 
			} else {
				*result_type = TDS_DONEINPROC_RESULT;
				return TDS_SUCCEED;
			}
			break;
		default:
			if (tds_process_default_tokens(tds, marker) == TDS_FAIL) {
				return TDS_FAIL;
			}
			break;
		}
	}
}

/**
 * process TDS row-type message streams.
 * tds_process_row_tokens() is called once a result set has been obtained
 * with tds_process_result_tokens(). It calls tds_process_row() to copy
 * data into the row buffer.
 * @param tds A pointer to the TDSSOCKET structure managing a 
 *    client/server operation.
 * @param rowtype A pointer to an integer variable which 
 *    tds_process_row_tokens sets to indicate the current type of row
 * @param computeid A pointer to an integer variable which 
 *    tds_process_row_tokens sets to identify the compute_id of the row 
 *    being returned. A compute row is a row that is generated by a 
 *    compute clause. The compute_id matches the number of the compute row 
 *    that was read; the first compute row is 1, the second is 2, and so forth.
 * @par Possible values of *rowtype
 *        @li @c TDS_REG_ROW      A regular data row
 *        @li @c TDS_COMP_ROW     A row of compute data
 *        @li @c TDS_NO_MORE_ROWS There are no more rows of data in this result set
 * @retval TDS_SUCCEED A row of data is available for processing.
 * @retval TDS_NO_MORE_ROWS All rows have been completely processed.
 * @retval TDS_FAIL An unexpected error occurred
 * @par Examples
 * The following code is cut from dbnextrow(), the db-library function
 * @include token2.c
 */
int
tds_process_row_tokens(TDSSOCKET * tds, TDS_INT * rowtype, TDS_INT * computeid)
{
	/* call internal function, with last parameter 1 */
	/* meaning read & process the end token          */

	return _tds_process_row_tokens(tds, rowtype, computeid, 1);
}
int
tds_process_row_tokens_ct(TDSSOCKET * tds, TDS_INT * rowtype, TDS_INT * computeid)
{
	/* call internal function, with last parameter 0 */
	/* meaning DON'T read & process the end token    */

	return _tds_process_row_tokens(tds, rowtype, computeid, 0);
}

static int
_tds_process_row_tokens(TDSSOCKET * tds, TDS_INT * rowtype, TDS_INT * computeid, TDS_INT read_end_token)
{
	int marker;

	if (IS_TDSDEAD(tds))
		return TDS_FAIL;
	if (tds->state == TDS_IDLE) {
		*rowtype = TDS_NO_MORE_ROWS;
		tdsdump_log(TDS_DBG_FUNC, "%L tds_process_row_tokens() state is COMPLETED\n");
		return TDS_NO_MORE_ROWS;
	}

	while (1) {

		marker = tds_get_byte(tds);
		tdsdump_log(TDS_DBG_INFO1, "%L processing row tokens.  marker is  %x(%s)\n", marker, _tds_token_name(marker));

		switch (marker) {
		case TDS_RESULT_TOKEN:
		case TDS_ROWFMT2_TOKEN:
		case TDS7_RESULT_TOKEN:

			tds_unget_byte(tds);
			*rowtype = TDS_NO_MORE_ROWS;
			return TDS_NO_MORE_ROWS;

		case TDS_ROW_TOKEN:
			if (tds_process_row(tds) == TDS_FAIL)
				return TDS_FAIL;

			*rowtype = TDS_REG_ROW;
			tds->curr_resinfo = tds->res_info;

			return TDS_SUCCEED;

		case TDS_CMP_ROW_TOKEN:

			*rowtype = TDS_COMP_ROW;
			return tds_process_compute(tds, computeid);

		case TDS_DONE_TOKEN:
		case TDS_DONEPROC_TOKEN:
		case TDS_DONEINPROC_TOKEN:
			if (read_end_token) {
				if (tds_process_end(tds, marker, NULL) == TDS_FAIL)
					return TDS_FAIL;
			} else {
				tds_unget_byte(tds);
			}
			*rowtype = TDS_NO_MORE_ROWS;
			return TDS_NO_MORE_ROWS;

		default:
			if (tds_process_default_tokens(tds, marker) == TDS_FAIL)
				return TDS_FAIL;
			break;
		}
	}
	return TDS_SUCCEED;
}

/**
 * tds_process_trailing_tokens() is called to discard messages that may
 * be left unprocessed at the end of a result "batch". In dblibrary, it is 
 * valid to process all the data rows that a command may have returned but 
 * to leave end tokens etc. unprocessed (at least explicitly)
 * This function is called to discard such tokens. If it comes across a token
 * that does not fall into the category of valid "trailing" tokens, it will 
 * return TDS_FAIL, allowing the calling dblibrary function to return a 
 * "results pending" message. 
 * The valid "trailing" tokens are :
 *
 * TDS_DONE_TOKEN
 * TDS_DONEPROC_TOKEN
 * TDS_DONEINPROC_TOKEN
 * TDS_RETURNSTATUS_TOKEN
 * TDS_PARAM_TOKEN
 * TDS5_PARAMFMT_TOKEN
 * TDS5_PARAMS_TOKEN
 */
int
tds_process_trailing_tokens(TDSSOCKET * tds)
{
	int marker;
	int done_flags;

	tdsdump_log(TDS_DBG_FUNC, "%L tds_process_trailing_tokens() \n");

	while (tds->state != TDS_IDLE) {

		marker = tds_get_byte(tds);
		tdsdump_log(TDS_DBG_INFO1, "%L processing trailing tokens.  marker is  %x(%s)\n", marker, _tds_token_name(marker));
		switch (marker) {
		case TDS_DONE_TOKEN:
		case TDS_DONEPROC_TOKEN:
		case TDS_DONEINPROC_TOKEN:
			tds_process_end(tds, marker, &done_flags);
			break;
		case TDS_RETURNSTATUS_TOKEN:
			tds->has_status = 1;
			tds->ret_status = tds_get_int(tds);
			break;
		case TDS_PARAM_TOKEN:
			tds_unget_byte(tds);
			tds_process_param_result_tokens(tds);
			break;
		case TDS5_PARAMFMT_TOKEN:
			tds_process_dyn_result(tds);
			break;
		case TDS5_PARAMFMT2_TOKEN:
			tds5_process_dyn_result2(tds);
			break;
		case TDS5_PARAMS_TOKEN:
			tds_process_params_result_token(tds);
			break;
		default:
			tds_unget_byte(tds);
			return TDS_FAIL;

		}
	}
	return TDS_SUCCEED;
}

/**
 * Process results for simple query as "SET TEXTSIZE" or "USE dbname"
 * If the statement returns results, beware they are discarded.
 *
 * This function was written to avoid direct calls to tds_process_default_tokens
 * (which caused problems such as ignoring query errors).
 * Results are read until idle state or severe failure (do not stop for 
 * statement failure).
 * @return see tds_process_result_tokens for results (TDS_NO_MORE_RESULTS is never returned)
 */
int
tds_process_simple_query(TDSSOCKET * tds)
{
TDS_INT res_type;
TDS_INT done_flags;
TDS_INT row_type;
int     rc;

	while ((rc = tds_process_result_tokens(tds, &res_type, &done_flags)) == TDS_SUCCEED) {
		switch (res_type) {

			case TDS_ROW_RESULT:
			case TDS_COMPUTE_RESULT:

				/* discard all this information */
				while ((rc = tds_process_row_tokens(tds, &row_type, NULL)) == TDS_SUCCEED);

				if (rc != TDS_NO_MORE_ROWS)
					return TDS_FAIL;

				break;

			case TDS_DONE_RESULT:
			case TDS_DONEPROC_RESULT:
			case TDS_DONEINPROC_RESULT:
                if ((done_flags & TDS_DONE_ERROR) != 0) 
					return TDS_FAIL;
				break;

			default:
				break;
		}
	}
	if (rc != TDS_NO_MORE_RESULTS) {
		return TDS_FAIL;
	}

    return TDS_SUCCEED;

}

/** 
 * simple flush function.  maybe be superseded soon.
 */
int
tds_do_until_done(TDSSOCKET * tds)
{
	int marker, rows_affected = 0;

	do {
		marker = tds_get_byte(tds);
		if (marker == TDS_DONE_TOKEN) {
			tds_process_end(tds, marker, NULL);
			rows_affected = tds->rows_affected;
		} else {
			tds_process_default_tokens(tds, marker);
		}
	} while (marker != TDS_DONE_TOKEN);

	return rows_affected;
}

/**
 * tds_process_col_name() is one half of the result set under TDS 4.2
 * it contains all the column names, a TDS_COLFMT_TOKEN should 
 * immediately follow this token with the datatype/size information
 * This is a 4.2 only function
 */
static int
tds_process_col_name(TDSSOCKET * tds)
{
	int hdrsize, len = 0;
	int memrc = 0;
	int col, num_cols = 0;
	struct tmp_col_struct
	{
		char *column_name;
		int column_namelen;
		struct tmp_col_struct *next;
	};
	struct tmp_col_struct *head = NULL, *cur = NULL, *prev;
	TDSCOLINFO *curcol;
	TDSRESULTINFO *info;

	hdrsize = tds_get_smallint(tds);

	/* this is a little messy...TDS 5.0 gives the number of columns
	 * upfront, while in TDS 4.2, you're expected to figure it out
	 * by the size of the message. So, I use a link list to get the
	 * colum names and then allocate the result structure, copy
	 * and delete the linked list */
	/* TODO: reallocate columns
	 * TODO code similar below, function to reuse */
	while (len < hdrsize) {
		prev = cur;
		cur = (struct tmp_col_struct *)
			malloc(sizeof(struct tmp_col_struct));

		if (!cur) {
			memrc = -1;
			break;
		}

		if (prev)
			prev->next = cur;
		if (!head)
			head = cur;

		cur->column_namelen = tds_get_byte(tds);
		memrc += tds_alloc_get_string(tds, &cur->column_name, cur->column_namelen);
		cur->next = NULL;

		len += cur->column_namelen + 1;
		num_cols++;
	}

	/* free results/computes/params etc... */
	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;

	if ((info = tds_alloc_results(num_cols)) == NULL)
		memrc = -1;
	tds->curr_resinfo = tds->res_info = info;
	/* tell the upper layers we are processing results */
	tds->state = TDS_PENDING;
	cur = head;

	if (memrc != 0) {
		while (cur != NULL) {
			prev = cur;
			cur = cur->next;
			free(prev->column_name);
			free(prev);
		}
		return TDS_FAIL;
	} else {
		for (col = 0; col < info->num_cols; col++) {
			curcol = info->columns[col];
			strncpy(curcol->column_name, cur->column_name, sizeof(curcol->column_name));
			curcol->column_name[sizeof(curcol->column_name) - 1] = 0;
			curcol->column_namelen = strlen(curcol->column_name);
			prev = cur;
			cur = cur->next;
			free(prev->column_name);
			free(prev);
		}
		return TDS_SUCCEED;
	}
}

/**
 * Add a column size to result info row size and calc offset into row
 * @param info   result where to add column
 * @param curcol column to add
 */
void
tds_add_row_column_size(TDSRESULTINFO * info, TDSCOLINFO * curcol)
{
	/* the column_offset is the offset into the row buffer
	 * where this column begins, text types are no longer
	 * stored in the row buffer because the max size can
	 * be too large (2gig) to allocate 
	 */
	curcol->column_offset = info->row_size;
	if (is_numeric_type(curcol->column_type)) {
		info->row_size += sizeof(TDS_NUMERIC);
	} else if (is_blob_type(curcol->column_type)) {
		info->row_size += sizeof(TDSBLOBINFO);
	} else {
		info->row_size += curcol->column_size;
	}
	info->row_size += (TDS_ALIGN_SIZE - 1);
	info->row_size -= info->row_size % TDS_ALIGN_SIZE;
}

/**
 * tds_process_col_fmt() is the other half of result set processing
 * under TDS 4.2. It follows tds_process_col_name(). It contains all the 
 * column type and size information.
 * This is a 4.2 only function
 */
static int
tds_process_col_fmt(TDSSOCKET * tds)
{
	int col, hdrsize;
	TDSCOLINFO *curcol;
	TDSRESULTINFO *info;
	TDS_SMALLINT tabnamesize;
	int bytes_read = 0;
	int rest;
	TDS_SMALLINT flags;

	hdrsize = tds_get_smallint(tds);

	/* TODO use curr_resinfo instead of res_info ?? */
	info = tds->res_info;
	for (col = 0; col < info->num_cols; col++) {
		curcol = info->columns[col];
		/* In Sybase all 4 byte are used for usertype, while mssql place 2 byte as usertype and 2 byte as flags */
		if (TDS_IS_MSSQL(tds)) {
			curcol->column_usertype = tds_get_smallint(tds);
			flags = tds_get_smallint(tds);
			curcol->column_nullable = flags & 0x01;
			curcol->column_writeable = (flags & 0x08) > 0;
			curcol->column_identity = (flags & 0x10) > 0;
		} else {
			curcol->column_usertype = tds_get_int(tds);
		}
		/* on with our regularly scheduled code (mlilback, 11/7/01) */
		tds_set_column_type(curcol, tds_get_byte(tds));

		tdsdump_log(TDS_DBG_INFO1, "%L processing result. type = %d(%s), varint_size %d\n",
			    curcol->column_type, tds_prtype(curcol->column_type), curcol->column_varint_size);

		switch (curcol->column_varint_size) {
		case 4:
			curcol->column_size = tds_get_int(tds);
			/* junk the table name -- for now */
			tabnamesize = tds_get_smallint(tds);
			tds_get_n(tds, NULL, tabnamesize);
			bytes_read += 5 + 4 + 2 + tabnamesize;
			break;
		case 1:
			curcol->column_size = tds_get_byte(tds);
			bytes_read += 5 + 1;
			break;
		case 0:
			bytes_read += 5 + 0;
			break;
		}

		/* Adjust column size according to client's encoding */
		curcol->on_server.column_size = curcol->column_size;
		adjust_character_column_size(tds, curcol);

		tds_add_row_column_size(info, curcol);
	}

	/* get the rest of the bytes */
	rest = hdrsize - bytes_read;
	if (rest > 0) {
		tdsdump_log(TDS_DBG_INFO1, "NOTE:tds_process_col_fmt: draining %d bytes\n", rest);
		tds_get_n(tds, NULL, rest);
	}

	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

static int
tds_process_colinfo(TDSSOCKET * tds)
{
	int hdrsize;
	TDSCOLINFO *curcol;
	TDSRESULTINFO *info;
	int bytes_read = 0;
	unsigned char col_info[3], l;

	hdrsize = tds_get_smallint(tds);

	/* TODO use curr_resinfo instead of res_info ?? */
	info = tds->res_info;
	while (bytes_read < hdrsize) {

		tds_get_n(tds, col_info, 3);
		bytes_read += 3;
		if (info && col_info[0] > 0 && col_info[0] <= info->num_cols) {
			curcol = info->columns[col_info[0] - 1];
			curcol->column_writeable = (col_info[2] & 0x4) == 0;
			curcol->column_key = (col_info[2] & 0x8) > 0;
			curcol->column_hidden = (col_info[2] & 0x10) > 0;
		}
		/* skip real name */
		/* TODO keep it */
		if (col_info[2] & 0x20) {
			l = tds_get_byte(tds);
			if (IS_TDS7_PLUS(tds))
				l *= 2;
			tds_get_n(tds, NULL, l);
			bytes_read += l + 1;
		}
	}

	return TDS_SUCCEED;
}

/**
 * tds_process_param_result() processes output parameters of a stored 
 * procedure. This differs from regular row/compute results in that there
 * is no total number of parameters given, they just show up singley.
 */
static int
tds_process_param_result(TDSSOCKET * tds, TDSPARAMINFO ** pinfo)
{
	int hdrsize;
	TDSCOLINFO *curparam;
	TDSPARAMINFO *info;
	int i;

	/* TODO check if curr_resinfo is a param result */

	/* limited to 64K but possible types are always smaller (not TEXT/IMAGE) */
	hdrsize = tds_get_smallint(tds);
	if ((info = tds_alloc_param_result(*pinfo)) == NULL)
		return TDS_FAIL;

	*pinfo = info;
	curparam = info->columns[info->num_cols - 1];

	/* FIXME check support for tds7+ (seem to use same format of tds5 for data...)
	 * perhaps varint_size can be 2 or collation can be specified ?? */
	tds_get_data_info(tds, curparam);

	curparam->column_cur_size = curparam->column_size;	/* needed ?? */

	if (tds_alloc_param_row(info, curparam) == NULL)
		return TDS_FAIL;

	i = tds_get_data(tds, curparam, info->current_row, info->num_cols - 1);

	return i;
}

static int
tds_process_param_result_tokens(TDSSOCKET * tds)
{
	int marker;
	TDSPARAMINFO **pinfo;

	if (tds->cur_dyn)
		pinfo = &(tds->cur_dyn->res_info);
	else
		pinfo = &(tds->param_info);

	while ((marker = tds_get_byte(tds)) == TDS_PARAM_TOKEN) {
		tds_process_param_result(tds, pinfo);
	}
	tds->curr_resinfo = *pinfo;
	tds_unget_byte(tds);
	return TDS_SUCCEED;
}

/**
 * tds_process_params_result_token() processes params on TDS5.
 */
static int
tds_process_params_result_token(TDSSOCKET * tds)
{
	int i;
	TDSCOLINFO *curcol;
	TDSPARAMINFO *info;

	/* TODO check if curr_resinfo is a param result */
	info = tds->curr_resinfo;
	if (!info)
		return TDS_FAIL;

	for (i = 0; i < info->num_cols; i++) {
		curcol = info->columns[i];
		if (tds_get_data(tds, curcol, info->current_row, i) != TDS_SUCCEED)
			return TDS_FAIL;
	}
	return TDS_SUCCEED;
}

/**
 * tds_process_compute_result() processes compute result sets.  These functions
 * need work but since they get little use, nobody has complained!
 * It is very similar to normal result sets.
 */
static int
tds_process_compute_result(TDSSOCKET * tds)
{
	int hdrsize;
	int col, num_cols;
	TDS_TINYINT by_cols = 0;
	TDS_TINYINT *cur_by_col;
	TDS_SMALLINT compute_id = 0;
	TDSCOLINFO *curcol;
	TDSCOMPUTEINFO *info;
	int i;


	hdrsize = tds_get_smallint(tds);

	/* compute statement id which this relates */
	/* to. You can have more than one compute  */
	/* statement in a SQL statement            */

	compute_id = tds_get_smallint(tds);

	tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. compute_id = %d\n", compute_id);

	/* number of compute columns returned - so */
	/* COMPUTE SUM(x), AVG(x)... would return  */
	/* num_cols = 2                            */

	num_cols = tds_get_byte(tds);

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return TDS_FAIL;
		info = tds->comp_info[i];
		tdsdump_log(TDS_DBG_FUNC, "%L in dbaltcolid() found computeid = %d\n", info->computeid);
		if (info->computeid == compute_id)
			break;
	}

	tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. num_cols = %d\n", num_cols);

	for (col = 0; col < num_cols; col++) {
		tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. point 2\n");
		curcol = info->columns[col];

		curcol->column_operator = tds_get_byte(tds);
		curcol->column_operand = tds_get_byte(tds);

		/* if no name has been defined for the compute column, */
		/* put in "max", "avg" etc.                            */

		if (curcol->column_namelen == 0) {
			strcpy(curcol->column_name, tds_pr_op(curcol->column_operator));
			curcol->column_namelen = strlen(curcol->column_name);
		}

		/*  User defined data type of the column */
		curcol->column_usertype = tds_get_int(tds);

		tds_set_column_type(curcol, tds_get_byte(tds));

		switch (curcol->column_varint_size) {
		case 4:
			curcol->column_size = tds_get_int(tds);
			break;
		case 2:
			curcol->column_size = tds_get_smallint(tds);
			break;
		case 1:
			curcol->column_size = tds_get_byte(tds);
			break;
		case 0:
			break;
		}
		tdsdump_log(TDS_DBG_INFO1, "%L processing result. column_size %d\n", curcol->column_size);

		/* Adjust column size according to client's encoding */
		curcol->on_server.column_size = curcol->column_size;
		/* TODO check if this column can have collation information associated */
		adjust_character_column_size(tds, curcol);

		/* skip locale */
		if (!IS_TDS42(tds))
			tds_get_n(tds, NULL, tds_get_byte(tds));

		tds_add_row_column_size(info, curcol);
	}

	by_cols = tds_get_byte(tds);

	tdsdump_log(TDS_DBG_INFO1, "%L processing tds compute result. by_cols = %d\n", by_cols);

	if (by_cols) {
		if ((info->bycolumns = (TDS_TINYINT *) malloc(by_cols)) == NULL)
			return TDS_FAIL;

		memset(info->bycolumns, '\0', by_cols);
	}
	info->by_cols = by_cols;

	cur_by_col = info->bycolumns;
	for (col = 0; col < by_cols; col++) {
		*cur_by_col = tds_get_byte(tds);
		cur_by_col++;
	}

	if ((info->current_row = tds_alloc_compute_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 * Read data information from wire
 * @param curcol column where to store information
 */
static int
tds7_get_data_info(TDSSOCKET * tds, TDSCOLINFO * curcol)
{
	int colnamelen;

	/*  User defined data type of the column */
	curcol->column_usertype = tds_get_smallint(tds);

	curcol->column_flags = tds_get_smallint(tds);	/*  Flags */

	curcol->column_nullable = curcol->column_flags & 0x01;
	curcol->column_writeable = (curcol->column_flags & 0x08) > 0;
	curcol->column_identity = (curcol->column_flags & 0x10) > 0;

	tds_set_column_type(curcol, tds_get_byte(tds));	/* sets "cardinal" type */

	switch (curcol->column_varint_size) {
	case 4:
		curcol->column_size = tds_get_int(tds);
		break;
	case 2:
		curcol->column_size = tds_get_smallint(tds);
		break;
	case 1:
		curcol->column_size = tds_get_byte(tds);
		break;
	case 0:
		break;
	}

	/* Adjust column size according to client's encoding */
	curcol->on_server.column_size = curcol->column_size;

	/* numeric and decimal have extra info */
	if (is_numeric_type(curcol->column_type)) {
		curcol->column_prec = tds_get_byte(tds);	/* precision */
		curcol->column_scale = tds_get_byte(tds);	/* scale */
	}

	if (IS_TDS80(tds) && is_collate_type(curcol->on_server.column_type)) {
		/* based on true type as sent by server */
		/* first 2 bytes are windows code (such as 0x409 for english)
		 * other 2 bytes ???
		 * last bytes is id in syscharsets */
		tds_get_n(tds, curcol->column_collation, 5);
		curcol->iconv_info = tds_iconv_from_lcid(tds, curcol->column_collation[1] * 256 + curcol->column_collation[0]);
	}

	adjust_character_column_size(tds, curcol);

	if (is_blob_type(curcol->column_type)) {
		curcol->table_namelen =
			tds_get_string(tds, tds_get_smallint(tds), curcol->table_name, sizeof(curcol->table_name) - 1);
	}

	/* under 7.0 lengths are number of characters not 
	 * number of bytes...tds_get_string handles this */
	colnamelen = tds_get_string(tds, tds_get_byte(tds), curcol->column_name, sizeof(curcol->column_name) - 1);
	curcol->column_name[colnamelen] = 0;
	curcol->column_namelen = colnamelen;

	tdsdump_log(TDS_DBG_INFO1, "%L tds7_get_data_info:%d: \n"
		    "\ttype = %d (%s)\n"
		    "\tcolumn_varint_size = %d\n"
		    "\tcolname = %s (%d bytes)\n"
		    "\tcolumn_size = %d (%d on server)\n",
		    __LINE__, curcol->column_type, tds_prtype(curcol->column_type), curcol->column_varint_size,
		    curcol->column_name, curcol->column_namelen, curcol->column_size, curcol->on_server.column_size);

	return TDS_SUCCEED;
}

/**
 * tds7_process_result() is the TDS 7.0 result set processing routine.  It 
 * is responsible for populating the tds->res_info structure.
 * This is a TDS 7.0 only function
 */
static int
tds7_process_result(TDSSOCKET * tds)
{
	int col, num_cols;
	TDSCOLINFO *curcol;
	TDSRESULTINFO *info;

	/* read number of columns and allocate the columns structure */

	num_cols = tds_get_smallint(tds);

	/* This can be a DUMMY results token from a cursor fetch */

	if (num_cols == -1) {
		tdsdump_log(TDS_DBG_INFO1, "%L processing TDS7 result. no meta data\n");
		return TDS_SUCCEED;
	}

	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;

	if ((tds->res_info = tds_alloc_results(num_cols)) == NULL)
		return TDS_FAIL;
	info = tds->res_info;
	tds->curr_resinfo = tds->res_info;

	/* tell the upper layers we are processing results */
	tds->state = TDS_PENDING;

	/* loop through the columns populating COLINFO struct from
	 * server response */
	for (col = 0; col < num_cols; col++) {

		curcol = info->columns[col];

		tds7_get_data_info(tds, curcol);

		tds_add_row_column_size(info, curcol);
	}

	/* all done now allocate a row for tds_process_row to use */
	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 * Read data information from wire
 * @param curcol column where to store information
 */
static int
tds_get_data_info(TDSSOCKET * tds, TDSCOLINFO * curcol)
{

	curcol->column_namelen = tds_get_string(tds, tds_get_byte(tds), curcol->column_name, sizeof(curcol->column_name) - 1);
	curcol->column_name[curcol->column_namelen] = '\0';

	curcol->column_flags = tds_get_byte(tds);	/*  Flags */
	/* TODO check if all flags are the same for all TDS versions */
	if (IS_TDS50(tds))
		curcol->column_hidden = curcol->column_flags & 0x1;
	curcol->column_key = (curcol->column_flags & 0x2) > 1;
	curcol->column_writeable = (curcol->column_flags & 0x10) > 1;
	curcol->column_nullable = (curcol->column_flags & 0x20) > 1;
	curcol->column_identity = (curcol->column_flags & 0x40) > 1;

	curcol->column_usertype = tds_get_int(tds);
	tds_set_column_type(curcol, tds_get_byte(tds));

	tdsdump_log(TDS_DBG_INFO1, "%L processing result. type = %d(%s), varint_size %d\n",
		    curcol->column_type, tds_prtype(curcol->column_type), curcol->column_varint_size);
	switch (curcol->column_varint_size) {
	case 4:
		curcol->column_size = tds_get_int(tds);
		curcol->table_namelen =
			tds_get_string(tds, tds_get_smallint(tds), curcol->table_name, sizeof(curcol->table_name) - 1);
		break;
	case 2:
		curcol->column_size = tds_get_smallint(tds);
		break;
	case 1:
		curcol->column_size = tds_get_byte(tds);
		break;
	case 0:
		break;
	}
	tdsdump_log(TDS_DBG_INFO1, "%L processing result. column_size %d\n", curcol->column_size);

	/* numeric and decimal have extra info */
	if (is_numeric_type(curcol->column_type)) {
		curcol->column_prec = tds_get_byte(tds);	/* precision */
		curcol->column_scale = tds_get_byte(tds);	/* scale */
	}

	/* read sql collation info */
	/* TODO: we should use it ! */
	if (IS_TDS80(tds) && is_collate_type(curcol->on_server.column_type)) {
		tds_get_n(tds, curcol->column_collation, 5);
		curcol->iconv_info = tds_iconv_from_lcid(tds, curcol->column_collation[1] * 256 + curcol->column_collation[0]);
	}

	/* Adjust column size according to client's encoding */
	curcol->on_server.column_size = curcol->column_size;
	adjust_character_column_size(tds, curcol);

	return TDS_SUCCEED;
}

/**
 * tds_process_result() is the TDS 5.0 result set processing routine.  It 
 * is responsible for populating the tds->res_info structure.
 * This is a TDS 5.0 only function
 */
static int
tds_process_result(TDSSOCKET * tds)
{
	int hdrsize;
	int col, num_cols;
	TDSCOLINFO *curcol;
	TDSRESULTINFO *info;

	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;

	hdrsize = tds_get_smallint(tds);

	/* read number of columns and allocate the columns structure */
	num_cols = tds_get_smallint(tds);
	if ((tds->res_info = tds_alloc_results(num_cols)) == NULL)
		return TDS_FAIL;

	info = tds->res_info;
	tds->curr_resinfo = tds->res_info;

	/* tell the upper layers we are processing results */
	tds->state = TDS_PENDING;

	/* loop through the columns populating COLINFO struct from
	 * server response */
	for (col = 0; col < info->num_cols; col++) {
		curcol = info->columns[col];

		tds_get_data_info(tds, curcol);

		/* skip locale information */
		/* NOTE do not put into tds_get_data_info, param do not have locale information */
		tds_get_n(tds, NULL, tds_get_byte(tds));

		tds_add_row_column_size(info, curcol);
	}
	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 * tds5_process_result() is the new TDS 5.0 result set processing routine.  
 * It is responsible for populating the tds->res_info structure.
 * This is a TDS 5.0 only function
 */
static int
tds5_process_result(TDSSOCKET * tds)
{
	int hdrsize;

	/* int colnamelen; */
	int col, num_cols;
	TDSCOLINFO *curcol;
	TDSRESULTINFO *info;

	tdsdump_log(TDS_DBG_INFO1, "%L tds5_process_result\n");

	/*
	 * free previous resultset
	 */
	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;

	/*
	 * read length of packet (4 bytes)
	 */
	hdrsize = tds_get_int(tds);

	/* read number of columns and allocate the columns structure */
	num_cols = tds_get_smallint(tds);
	if ((tds->res_info = tds_alloc_results(num_cols)) == NULL)
		return TDS_FAIL;
	info = tds->res_info;

	tdsdump_log(TDS_DBG_INFO1, "%L num_cols=%d\n", num_cols);

	/* tell the upper layers we are processing results */
	tds->state = TDS_PENDING;

	/* TODO reuse some code... */
	/* loop through the columns populating COLINFO struct from
	 * server response */
	for (col = 0; col < info->num_cols; col++) {
		curcol = info->columns[col];

		/* label */
		curcol->column_namelen =
			tds_get_string(tds, tds_get_byte(tds), curcol->column_name, sizeof(curcol->column_name) - 1);
		curcol->column_name[curcol->column_namelen] = '\0';

		/* TODO add these field again */
		/* database */
		/*
		 * colnamelen = tds_get_byte(tds);
		 * tds_get_n(tds, curcol->catalog_name, colnamelen);
		 * curcol->catalog_name[colnamelen] = '\0';
		 */

		/* owner */
		/*
		 * colnamelen = tds_get_byte(tds);
		 * tds_get_n(tds, curcol->schema_name, colnamelen);
		 * curcol->schema_name[colnamelen] = '\0';
		 */

		/* table */
		/*
		 * colnamelen = tds_get_byte(tds);
		 * tds_get_n(tds, curcol->table_name, colnamelen);
		 * curcol->table_name[colnamelen] = '\0';
		 */

		/* column name */
		/*
		 * colnamelen = tds_get_byte(tds);
		 * tds_get_n(tds, curcol->column_colname, colnamelen);
		 * curcol->column_colname[colnamelen] = '\0';
		 */

		/* if label is empty, use the column name */
		/*
		 * if (colnamelen > 0 && curcol->column_name[0] == '\0')
		 * strcpy(curcol->column_name, curcol->column_colname);
		 */

		/* flags (4 bytes) */
		curcol->column_flags = tds_get_int(tds);
		curcol->column_hidden = curcol->column_flags & 0x1;
		curcol->column_key = (curcol->column_flags & 0x2) > 1;
		curcol->column_writeable = (curcol->column_flags & 0x10) > 1;
		curcol->column_nullable = (curcol->column_flags & 0x20) > 1;
		curcol->column_identity = (curcol->column_flags & 0x40) > 1;

		curcol->column_usertype = tds_get_int(tds);

		curcol->column_type = tds_get_byte(tds);

		curcol->column_varint_size = tds5_get_varint_size(curcol->column_type);

		switch (curcol->column_varint_size) {
		case 4:
			if (curcol->column_type == SYBTEXT || curcol->column_type == SYBIMAGE) {
				int namelen;

				curcol->column_size = tds_get_int(tds);

				/* skip name */
				namelen = tds_get_smallint(tds);
				if (namelen)
					tds_get_n(tds, NULL, namelen);

			} else
				tdsdump_log(TDS_DBG_INFO1, "%L UNHANDLED TYPE %x\n", curcol->column_type);
			break;
		case 5:
			curcol->column_size = tds_get_int(tds);
			break;
		case 2:
			curcol->column_size = tds_get_smallint(tds);
			break;
		case 1:
			curcol->column_size = tds_get_byte(tds);
			break;
		case 0:
			curcol->column_size = tds_get_size_by_type(curcol->column_type);
			break;
		}

		/* Adjust column size according to client's encoding */
		curcol->on_server.column_size = curcol->column_size;
		adjust_character_column_size(tds, curcol);

		/* numeric and decimal have extra info */
		if (is_numeric_type(curcol->column_type)) {
			curcol->column_prec = tds_get_byte(tds);	/* precision */
			curcol->column_scale = tds_get_byte(tds);	/* scale */
		}

		/* discard Locale */
		tds_get_n(tds, NULL, tds_get_byte(tds));

		tds_add_row_column_size(info, curcol);

		/* 
		 *  Dump all information on this column
		 */
		tdsdump_log(TDS_DBG_INFO1, "%L col %d:\n", col);
		tdsdump_log(TDS_DBG_INFO1, "%L\tcolumn_label=[%s]\n", curcol->column_name);
/*		tdsdump_log(TDS_DBG_INFO1, "%L\tcolumn_name=[%s]\n", curcol->column_colname);
		tdsdump_log(TDS_DBG_INFO1, "%L\tcatalog=[%s] schema=[%s] table=[%s]\n",
			    curcol->catalog_name, curcol->schema_name, curcol->table_name, curcol->column_colname);
*/
		tdsdump_log(TDS_DBG_INFO1, "%L\tflags=%x utype=%d type=%d varint=%d\n",
			    curcol->column_flags, curcol->column_usertype, curcol->column_type, curcol->column_varint_size);

		tdsdump_log(TDS_DBG_INFO1, "%L\tcolsize=%d prec=%d scale=%d\n",
			    curcol->column_size, curcol->column_prec, curcol->column_scale);
	}
	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 * tds_process_compute() processes compute rows and places them in the row
 * buffer.  
 */
static int
tds_process_compute(TDSSOCKET * tds, TDS_INT * computeid)
{
	int i;
	TDSCOLINFO *curcol;
	TDSCOMPUTEINFO *info;
	TDS_INT compute_id;

	compute_id = tds_get_smallint(tds);

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return TDS_FAIL;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}
	tds->curr_resinfo = info;

	for (i = 0; i < info->num_cols; i++) {
		curcol = info->columns[i];
		if (tds_get_data(tds, curcol, info->current_row, i) != TDS_SUCCEED)
			return TDS_FAIL;
	}
	if (computeid)
		*computeid = compute_id;
	return TDS_SUCCEED;
}


/**
 * Read a data from wire
 * @param curcol column where store column information
 * @param pointer to row data to store information
 * @param i column position in current_row
 * @return TDS_FAIL on error or TDS_SUCCEED
 */
static int
tds_get_data(TDSSOCKET * tds, TDSCOLINFO * curcol, unsigned char *current_row, int i)
{
	unsigned char *dest;
	int len, colsize;
	int fillchar;
	TDSBLOBINFO *blob_info;

	tdsdump_log(TDS_DBG_INFO1, "%L processing row.  column is %d varint size = %d\n", i, curcol->column_varint_size);
	switch (curcol->column_varint_size) {
	case 4:
		/* TODO finish 
		 * This strange type has following structure 
		 * 0 len (int32) -- NULL 
		 * len (int32), type (int8), data -- ints, date, etc
		 * len (int32), type (int8), 7 (int8), collation, column size (int16) -- [n]char, [n]varchar, binary, varbianry 
		 * BLOBS (text/image) not supported */
		if (curcol->column_type == SYBVARIANT) {
			colsize = tds_get_int(tds);
			tds_get_n(tds, NULL, colsize);
			tds_set_null(current_row, i);
			return TDS_SUCCEED;
		}
		/* Its a BLOB... */
		len = tds_get_byte(tds);
		blob_info = (TDSBLOBINFO *) & (current_row[curcol->column_offset]);
		if (len == 16) {	/*  Jeff's hack */
			tds_get_n(tds, blob_info->textptr, 16);
			tds_get_n(tds, blob_info->timestamp, 8);
			colsize = tds_get_int(tds);
		} else {
			colsize = 0;
		}
		break;
	case 2:
		colsize = tds_get_smallint(tds);
		/* handle empty no-NULL string */
		if (colsize == 0) {
			tds_clr_null(current_row, i);
			curcol->column_cur_size = 0;
			return TDS_SUCCEED;
		}
		if (colsize == -1)
			colsize = 0;
		break;
	case 1:
		colsize = tds_get_byte(tds);
		break;
	case 0:
		/* TODO this should be column_size */
		colsize = tds_get_size_by_type(curcol->column_type);
		break;
	default:
		colsize = 0;
		break;
	}
	if (IS_TDSDEAD(tds))
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "%L processing row.  column size is %d \n", colsize);
	/* set NULL flag in the row buffer */
	if (colsize == 0) {
		tds_set_null(current_row, i);
		return TDS_SUCCEED;
	}

	tds_clr_null(current_row, i);

	dest = &(current_row[curcol->column_offset]);
	if (is_numeric_type(curcol->column_type)) {
		TDS_NUMERIC *num;

		/* 
		 * handling NUMERIC datatypes: 
		 * since these can be passed around independent
		 * of the original column they were from, I decided
		 * to embed the TDS_NUMERIC datatype in the row buffer
		 * instead of using the wire representation even though
		 * it uses a few more bytes
		 */
		num = (TDS_NUMERIC *) dest;
		memset(num, '\0', sizeof(TDS_NUMERIC));
		num->precision = curcol->column_prec;
		num->scale = curcol->column_scale;

		/* server is going to crash freetds ?? */
		if (colsize > sizeof(num->array))
			return TDS_FAIL;
		tds_get_n(tds, num->array, colsize);

		/* corrected colsize for column_cur_size */
		colsize = sizeof(TDS_NUMERIC);
		if (IS_TDS7_PLUS(tds)) {
			tdsdump_log(TDS_DBG_INFO1, "%L swapping numeric data...\n");
			tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), (unsigned char *) num);
		}

	} else if (is_blob_type(curcol->column_type)) {
		TDS_CHAR *p;

		/* This seems wrong.  text and image have the same wire format, 
		 * but I don't see any reason to convert image data.  --jkl
		 */
		blob_info = (TDSBLOBINFO *) dest;

		p = blob_info->textvalue;
		if (!p) {
			p = (TDS_CHAR *) malloc(colsize);
		} else {
			p = (TDS_CHAR *) realloc(p, colsize);
		}
		if (!p)
			return TDS_FAIL;
		blob_info->textvalue = p;
		if (is_char_type(curcol->column_type)) {
			curcol->column_cur_size = colsize;
			/* FIXME: test error */
			if (tds_get_char_data(tds, (char *) blob_info, colsize, curcol) == TDS_FAIL)
				return TDS_FAIL;
			/* just to make happy code below ... */
			colsize = curcol->column_cur_size;
		} else
			tds_get_n(tds, blob_info->textvalue, colsize);
	} else {		/* non-numeric and non-blob */
		if (is_char_type(curcol->column_type)) {
			/* this shouldn't fail here */
			if (tds_get_char_data(tds, (char *) dest, colsize, curcol) == TDS_FAIL)
				return TDS_FAIL;
			/* just to make happy code below ... */
			colsize = curcol->column_cur_size;
		} else {
			if (colsize > curcol->column_size)
				return TDS_FAIL;
			if (tds_get_n(tds, dest, colsize) == NULL)
				return TDS_FAIL;
		}

		/* pad CHAR and BINARY types */
		fillchar = 0;
		switch (curcol->column_type) {
		case SYBCHAR:
		case XSYBCHAR:
			if (curcol->column_size != curcol->on_server.column_size)
				break;
			/* FIXME use client charset */
			fillchar = ' ';
		case SYBBINARY:
		case XSYBBINARY:
			if (colsize < curcol->column_size)
				memset(dest + colsize, fillchar, curcol->column_size - colsize);
			colsize = curcol->column_size;
			break;
		}

		if (curcol->column_type == SYBDATETIME4) {
			tdsdump_log(TDS_DBG_INFO1, "%L datetime4 %d %d %d %d\n", dest[0], dest[1], dest[2], dest[3]);
		}
	}

	/* Value used to properly know value in dbdatlen. (mlilback, 11/7/01) */
	curcol->column_cur_size = colsize;

#ifdef WORDS_BIGENDIAN
	/* MS SQL Server 7.0 has broken date types from big endian 
	 * machines, this swaps the low and high halves of the 
	 * affected datatypes
	 *
	 * Thought - this might be because we don't have the
	 * right flags set on login.  -mjs
	 *
	 * Nope its an actual MS SQL bug -bsb
	 */
	if (tds->broken_dates &&
	    (curcol->column_type == SYBDATETIME ||
	     curcol->column_type == SYBDATETIME4 ||
	     curcol->column_type == SYBDATETIMN ||
	     curcol->column_type == SYBMONEY ||
	     curcol->column_type == SYBMONEY4 || (curcol->column_type == SYBMONEYN && curcol->column_size > 4)))
		/* above line changed -- don't want this for 4 byte SYBMONEYN 
		 * values (mlilback, 11/7/01) */
	{
		unsigned char temp_buf[8];

		memcpy(temp_buf, dest, colsize / 2);
		memcpy(dest, &dest[colsize / 2], colsize / 2);
		memcpy(&dest[colsize / 2], temp_buf, colsize / 2);
	}
	if (tds->emul_little_endian && !is_numeric_type(curcol->column_type)) {
		tdsdump_log(TDS_DBG_INFO1, "%L swapping coltype %d\n", tds_get_conversion_type(curcol->column_type, colsize));
		tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), dest);
	}
#endif
	return TDS_SUCCEED;
}

/**
 * tds_process_row() processes rows and places them in the row buffer.
 */
static int
tds_process_row(TDSSOCKET * tds)
{
	int i;
	TDSCOLINFO *curcol;
	TDSRESULTINFO *info;

	/* TODO use curr_resinfo ?? */
	info = tds->res_info;
	if (!info)
		return TDS_FAIL;

	tds->curr_resinfo = info;

	info->row_count++;
	for (i = 0; i < info->num_cols; i++) {
		curcol = info->columns[i];
		if (tds_get_data(tds, curcol, info->current_row, i) != TDS_SUCCEED)
			return TDS_FAIL;
	}
	return TDS_SUCCEED;
}

/**
 * tds_process_end() processes any of the DONE, DONEPROC, or DONEINPROC
 * tokens.
 * @param marker     TDS token number
 * @param flags_parm filled with bit flags (see TDS_DONE_ constants). 
 *        Is NULL nothing is returned
 */
static TDS_INT
tds_process_end(TDSSOCKET * tds, int marker, int *flags_parm)
{
	int more_results, was_cancelled, error, done_count_valid;
	int tmp;

	tmp = tds_get_smallint(tds);

	more_results = (tmp & TDS_DONE_MORE_RESULTS) != 0;
	was_cancelled = (tmp & TDS_DONE_CANCELLED) != 0;
	error = (tmp & TDS_DONE_ERROR) != 0;
	done_count_valid = (tmp & TDS_DONE_COUNT) != 0;


	tdsdump_log(TDS_DBG_FUNC, "%L tds_process_end: more_results = %d\n"
		    "%L                  was_cancelled = %d\n"
		    "%L                  error = %d\n"
		    "%L                  done_count_valid = %d\n", more_results, was_cancelled, error, done_count_valid);

	if (tds->res_info) {
		tds->res_info->more_results = more_results;
		if (tds->curr_resinfo == NULL)
			tds->curr_resinfo = tds->res_info;

	}

	if (flags_parm)
		*flags_parm = tmp;

	if (was_cancelled || !(more_results)) {
		tdsdump_log(TDS_DBG_FUNC, "%L tds_process_end() state set to TDS_IDLE\n");
		tds->state = TDS_IDLE;
	}

	tds_get_smallint(tds);

	if (IS_TDSDEAD(tds))
		return TDS_FAIL;

	/* rows affected is in the tds struct because a query may affect rows but
	 * have no result set. */

	if (done_count_valid) {
		tds->rows_affected = tds_get_int(tds);
		tdsdump_log(TDS_DBG_FUNC, "%L                  rows_affected = %d\n", tds->rows_affected);
	} else {
		tmp = tds_get_int(tds);	/* throw it away */
		tds->rows_affected = TDS_NO_COUNT;
	}

	if (IS_TDSDEAD(tds))
		return TDS_FAIL;

	return TDS_SUCCEED;
}



/**
 * tds_client_msg() sends a message to the client application from the CLI or
 * TDS layer. A client message is one that is generated from with the library
 * and not from the server.  The message is sent to the CLI (the 
 * err_handler) so that it may forward it to the client application or
 * discard it if no msg handler has been by the application. tds->parent
 * contains a void pointer to the parent of the tds socket. This can be cast
 * back into DBPROCESS or CS_CONNECTION by the CLI and used to determine the
 * proper recipient function for this message.
 * \todo This procedure is deprecated, because the client libraries use differing messages and message numbers.
 * 	The general approach is to emit ct-lib error information and let db-lib and ODBC map that to their number and text.  
 */
int
tds_client_msg(TDSCONTEXT * tds_ctx, TDSSOCKET * tds, int msgnum, int level, int state, int line, const char *message)
{
	int ret;
	TDSMSGINFO msg_info;

	if (tds_ctx->err_handler) {
		memset(&msg_info, 0, sizeof(TDSMSGINFO));
		msg_info.msg_number = msgnum;
		msg_info.msg_level = level;	/* severity? */
		msg_info.msg_state = state;
		/* TODO is possible to avoid copy of strings ? */
		msg_info.server = strdup("OpenClient");
		msg_info.line_number = line;
		msg_info.message = strdup(message);
		if (msg_info.sql_state == NULL)
			msg_info.sql_state = tds_alloc_client_sqlstate(msg_info.msg_number);
		ret = tds_ctx->err_handler(tds_ctx, tds, &msg_info);
		tds_free_msg(&msg_info);
		/* message handler returned FAIL/CS_FAIL
		 * mark socket as dead */
		if (ret && tds) {
			/* TODO close socket too ?? */
			tds->state = TDS_DEAD;
		}
	}
	return 0;
}

/**
 * tds_process_env_chg() 
 * when ever certain things change on the server, such as database, character
 * set, language, or block size.  A environment change message is generated
 * There is no action taken currently, but certain functions at the CLI level
 * that return the name of the current database will need to use this.
 */
static int
tds_process_env_chg(TDSSOCKET * tds)
{
	int size, type;
	char *oldval = NULL;
	char *newval = NULL;
	char **dest;
	int new_block_size;
	int lcid;
	int memrc = 0;

	size = tds_get_smallint(tds);
	/* this came in a patch, apparently someone saw an env message
	 * that was different from what we are handling? -- brian
	 * changed back because it won't handle multibyte chars -- 7.0
	 */
	/* tds_get_n(tds,NULL,size); */

	type = tds_get_byte(tds);

	/* handle collate default change (if you change db or during login) 
	 * this environment is not a string so need different handles */
	if (type == TDS_ENV_SQLCOLLATION) {
		/* save new collation */
		size = tds_get_byte(tds);
		memset(tds->collation, 0, 5);
		if (size < 5) {
			tds_get_n(tds, tds->collation, size);
		} else {
			tds_get_n(tds, tds->collation, 5);
			tds_get_n(tds, NULL, size - 5);
			lcid = (tds->collation[0] + ((int) tds->collation[1] << 8) + ((int) tds->collation[2] << 16)) & 0xffffflu;
			tds7_srv_charset_changed(tds, lcid);
		}
		/* discard old one */
		tds_get_n(tds, NULL, tds_get_byte(tds));
		return TDS_SUCCEED;
	}

	/* fetch the new value */
	memrc += tds_alloc_get_string(tds, &newval, tds_get_byte(tds));

	/* fetch the old value */
	memrc += tds_alloc_get_string(tds, &oldval, tds_get_byte(tds));

	if (memrc != 0) {
		if (newval != NULL)
			free(newval);
		if (oldval != NULL)
			free(oldval);
		return TDS_FAIL;
	}

	dest = NULL;
	switch (type) {
	case TDS_ENV_PACKSIZE:
		new_block_size = atoi(newval);
		if (new_block_size > tds->env->block_size) {
			tdsdump_log(TDS_DBG_INFO1, "%L increasing block size from %s to %d\n", oldval, new_block_size);
			/* 
			 * I'm not aware of any way to shrink the 
			 * block size but if it is possible, we don't 
			 * handle it.
			 */
			/* Reallocate buffer if impossible (strange values from server or out of memory) use older buffer */
			tds_realloc_socket(tds, new_block_size);
		}
		break;
	case TDS_ENV_DATABASE:
		dest = &tds->env->database;
		break;
	case TDS_ENV_LANG:
		dest = &tds->env->language;
		break;
	case TDS_ENV_CHARSET:
		dest = &tds->env->charset;
		tds_srv_charset_changed(tds, newval);
		break;
	}
	if (tds->env_chg_func) {
		(*(tds->env_chg_func)) (tds, type, oldval, newval);
	}

	if (oldval)
		free(oldval);
	if (newval) {
		if (dest) {
			if (*dest)
				free(*dest);
			*dest = newval;
		} else
			free(newval);
	}

	return TDS_SUCCEED;
}

/**
 * tds_process_msg() is called for MSG, ERR, or EED tokens and is responsible
 * for calling the CLI's message handling routine
 * returns TDS_SUCCEED if informational, TDS_ERROR if error.
 */
static int
tds_process_msg(TDSSOCKET * tds, int marker)
{
	int rc;
	int len;
	int len_sqlstate;
	TDSMSGINFO msg_info;

	/* make sure message has been freed */
	memset(&msg_info, 0, sizeof(TDSMSGINFO));

	/* packet length */
	len = tds_get_smallint(tds);

	/* message number */
	rc = tds_get_int(tds);
	msg_info.msg_number = rc;

	/* msg state */
	msg_info.msg_state = tds_get_byte(tds);

	/* msg level */
	msg_info.msg_level = tds_get_byte(tds);

	/* determine if msg or error */
	switch (marker) {
	case TDS_EED_TOKEN:
		if (msg_info.msg_level <= 10)
			msg_info.priv_msg_type = 0;
		else
			msg_info.priv_msg_type = 1;

		/* read SQL state */
		len_sqlstate = tds_get_byte(tds);
		msg_info.sql_state = (char *) malloc(len_sqlstate + 1);
		if (!msg_info.sql_state) {
			tds_free_msg(&msg_info);
			return TDS_FAIL;
		}

		tds_get_n(tds, msg_info.sql_state, len_sqlstate);
		msg_info.sql_state[len_sqlstate] = '\0';

		/* do a better mapping using native errors */
		if (strcmp(msg_info.sql_state, "ZZZZZ") == 0)
			TDS_ZERO_FREE(msg_info.sql_state);

		/* junk status and transaction state */
		/* TODO if status == 1 clear cur_dyn and param_info ?? */
		tds_get_byte(tds);
		tds_get_smallint(tds);

		/* EED can be followed to PARAMFMT/PARAMS, do not store it in dynamic */
		tds->cur_dyn = NULL;
		break;
	case TDS_INFO_TOKEN:
		msg_info.priv_msg_type = 0;
		break;
	case TDS_ERROR_TOKEN:
		msg_info.priv_msg_type = 1;
		break;
	default:
		tdsdump_log(TDS_DBG_ERROR, "__FILE__:__LINE__: tds_process_msg() called with unknown marker '%d'!\n", (int) marker);
		tds_free_msg(&msg_info);
		return TDS_FAIL;
	}

	rc = 0;
	/* the message */
	rc += tds_alloc_get_string(tds, &msg_info.message, tds_get_smallint(tds));

	/* server name */
	rc += tds_alloc_get_string(tds, &msg_info.server, tds_get_byte(tds));

	/* stored proc name if available */
	rc += tds_alloc_get_string(tds, &msg_info.proc_name, tds_get_byte(tds));

	/* line number in the sql statement where the problem occured */
	msg_info.line_number = tds_get_smallint(tds);

	/* If the server doesen't provide an sqlstate, map one via server native errors
	 * I'm assuming there is not a protocol I'm missing to fetch these from the server?
	 * I know sybase has an sqlstate column in it's sysmessages table, mssql doesn't and
	 * TDS_EED_TOKEN is not being called for me. */
	if (msg_info.sql_state == NULL)
		msg_info.sql_state = tds_alloc_lookup_sqlstate(tds, msg_info.msg_number);

	/* call the msg_handler that was set by an upper layer 
	 * (dblib, ctlib or some other one).  Call it with the pointer to 
	 * the "parent" structure.
	 */

	if (rc != 0) {
		tds_free_msg(&msg_info);
		return TDS_ERROR;
	}

	if (tds->tds_ctx->msg_handler) {
		tds->tds_ctx->msg_handler(tds->tds_ctx, tds, &msg_info);
	} else {
		if (msg_info.msg_number)
			tdsdump_log(TDS_DBG_WARN,
				    "%L Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n",
				    msg_info.msg_number,
				    msg_info.msg_level,
				    msg_info.msg_state, msg_info.server, msg_info.line_number, msg_info.message);
	}
	tds_free_msg(&msg_info);
	return TDS_SUCCEED;
}

/**
 * Read a string from wire in a new allocated buffer
 * @param len length of string to read
 */
int
tds_alloc_get_string(TDSSOCKET * tds, char **string, int len)
{
	char *s;
	int out_len;

	if (len < 0) {
		*string = NULL;
		return 0;
	}

	/* assure sufficient space for evry conversion */
	s = (char *) malloc(len * 4 + 1);
	out_len = tds_get_string(tds, len, s, len * 4);
	if (!s) {
		*string = NULL;
		return -1;
	}
	s = realloc(s, out_len + 1);
	s[out_len] = '\0';
	*string = s;
	return 0;
}

/**
 * tds_process_cancel() processes the incoming token stream until it finds
 * an end token (DONE, DONEPROC, DONEINPROC) with the cancel flag set.
 * a that point the connetion should be ready to handle a new query.
 */
int
tds_process_cancel(TDSSOCKET * tds)
{
	int marker, done_flags = 0;
	int retcode = TDS_SUCCEED;

	tds->queryStarttime = 0;
	/* TODO support TDS5 cancel, wait for cancel packet first, then wait for done */
	do {
		marker = tds_get_byte(tds);
		if (marker == TDS_DONE_TOKEN) {
			{
				if (tds_process_end(tds, marker, &done_flags) == TDS_FAIL)
					retcode = TDS_FAIL;
			}
		} else if (marker == 0) {
			done_flags = TDS_DONE_CANCELLED;
		} else {
			tds_process_default_tokens(tds, marker);
		}
	} while (retcode == TDS_SUCCEED && !(done_flags & TDS_DONE_CANCELLED));


	if (retcode == TDS_SUCCEED && !IS_TDSDEAD(tds))
		tds->state = TDS_IDLE;
	else
		retcode = TDS_FAIL;

	/* TODO clear cancelled results */
	return retcode;
}

/**
 * set the null bit for the given column in the row buffer
 */
void
tds_set_null(unsigned char *current_row, int column)
{
	int bytenum = ((unsigned int) column) / 8u;
	int bit = ((unsigned int) column) % 8u;
	unsigned char mask = 1 << bit;

	tdsdump_log(TDS_DBG_INFO1, "%L setting column %d NULL bit\n", column);
	current_row[bytenum] |= mask;
}

/**
 * clear the null bit for the given column in the row buffer
 */
void
tds_clr_null(unsigned char *current_row, int column)
{
	int bytenum = ((unsigned int) column) / 8u;
	int bit = ((unsigned int) column) % 8u;
	unsigned char mask = ~(1 << bit);

	tdsdump_log(TDS_DBG_INFO1, "%L clearing column %d NULL bit\n", column);
	current_row[bytenum] &= mask;
}

/**
 * Return the null bit for the given column in the row buffer
 */
int
tds_get_null(unsigned char *current_row, int column)
{
	int bytenum = ((unsigned int) column) / 8u;
	int bit = ((unsigned int) column) % 8u;

	return (current_row[bytenum] >> bit) & 1;
}

/**
 * Find a dynamic given string id
 * @return dynamic or NULL is not found
 * @param id   dynamic id to search
 */
TDSDYNAMIC *
tds_lookup_dynamic(TDSSOCKET * tds, char *id)
{
	int i;

	for (i = 0; i < tds->num_dyns; i++) {
		if (!strcmp(tds->dyns[i]->id, id)) {
			return tds->dyns[i];
		}
	}
	return NULL;
}

/**
 * tds_process_dynamic()
 * finds the element of the dyns array for the id
 */
static TDSDYNAMIC *
tds_process_dynamic(TDSSOCKET * tds)
{
	int token_sz;
	unsigned char type, status;
	int id_len;
	char id[TDS_MAX_DYNID_LEN + 1];
	int drain = 0;

	token_sz = tds_get_smallint(tds);
	type = tds_get_byte(tds);
	status = tds_get_byte(tds);
	/* handle only acknowledge */
	if (type != 0x20) {
		tdsdump_log(TDS_DBG_ERROR, "Unrecognized TDS5_DYN type %x\n", type);
		tds_get_n(tds, NULL, token_sz - 2);
		return NULL;
	}
	id_len = tds_get_byte(tds);
	if (id_len > TDS_MAX_DYNID_LEN) {
		drain = id_len - TDS_MAX_DYNID_LEN;
		id_len = TDS_MAX_DYNID_LEN;
	}
	id_len = tds_get_string(tds, id_len, id, TDS_MAX_DYNID_LEN);
	id[id_len] = '\0';
	if (drain) {
		tds_get_string(tds, drain, NULL, drain);
	}
	return tds_lookup_dynamic(tds, id);
}

static int
tds_process_dyn_result(TDSSOCKET * tds)
{
	int hdrsize;
	int col, num_cols;
	TDSCOLINFO *curcol;
	TDSPARAMINFO *info;
	TDSDYNAMIC *dyn;

	hdrsize = tds_get_smallint(tds);
	num_cols = tds_get_smallint(tds);

	if (tds->cur_dyn) {
		dyn = tds->cur_dyn;
		tds_free_param_results(dyn->res_info);
		/* read number of columns and allocate the columns structure */
		if ((dyn->res_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = dyn->res_info;
	} else {
		tds_free_param_results(tds->param_info);
		if ((tds->param_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = tds->param_info;
	}
	tds->curr_resinfo = info;

	for (col = 0; col < info->num_cols; col++) {
		curcol = info->columns[col];

		tds_get_data_info(tds, curcol);

		/* skip locale information */
		tds_get_n(tds, NULL, tds_get_byte(tds));

		tds_add_row_column_size(info, curcol);
	}

	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 *  New TDS 5.0 token for describing output parameters
 */
static int
tds5_process_dyn_result2(TDSSOCKET * tds)
{
	int hdrsize;
	int col, num_cols;
	TDSCOLINFO *curcol;
	TDSPARAMINFO *info;
	TDSDYNAMIC *dyn;

	hdrsize = tds_get_int(tds);
	num_cols = tds_get_smallint(tds);

	if (tds->cur_dyn) {
		dyn = tds->cur_dyn;
		tds_free_param_results(dyn->res_info);
		/* read number of columns and allocate the columns structure */
		if ((dyn->res_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = dyn->res_info;
	} else {
		tds_free_param_results(tds->param_info);
		if ((tds->param_info = tds_alloc_results(num_cols)) == NULL)
			return TDS_FAIL;
		info = tds->param_info;
	}
	tds->curr_resinfo = info;

	for (col = 0; col < info->num_cols; col++) {
		curcol = info->columns[col];

		/* TODO reuse tds_get_data_info code, sligthly different */

		/* column name */
		curcol->column_namelen =
			tds_get_string(tds, tds_get_byte(tds), curcol->column_name, sizeof(curcol->column_name) - 1);
		curcol->column_name[curcol->column_namelen] = '\0';

		/* column status */
		curcol->column_flags = tds_get_int(tds);
		curcol->column_nullable = (curcol->column_flags & 0x20) > 0;

		/* user type */
		curcol->column_usertype = tds_get_int(tds);

		/* column type */
		tds_set_column_type(curcol, tds_get_byte(tds));

		/* FIXME this should be done by tds_set_column_type */
		curcol->column_varint_size = tds5_get_varint_size(curcol->column_type);
		/* column size */
		switch (curcol->column_varint_size) {
		case 5:
			curcol->column_size = tds_get_int(tds);
			break;
		case 4:
			if (curcol->column_type == SYBTEXT || curcol->column_type == SYBIMAGE) {
				curcol->column_size = tds_get_int(tds);
				/* read table name */
				curcol->table_namelen =
					tds_get_string(tds, tds_get_smallint(tds), curcol->table_name,
						       sizeof(curcol->table_name) - 1);
			} else
				tdsdump_log(TDS_DBG_INFO1, "%L UNHANDLED TYPE %x\n", curcol->column_type);
			break;
		case 2:
			curcol->column_size = tds_get_smallint(tds);
			break;
		case 1:
			curcol->column_size = tds_get_byte(tds);
			break;
		case 0:
			break;
		}

		/* Adjust column size according to client's encoding */
		curcol->on_server.column_size = curcol->column_size;
		adjust_character_column_size(tds, curcol);

		/* numeric and decimal have extra info */
		if (is_numeric_type(curcol->column_type)) {
			curcol->column_prec = tds_get_byte(tds);	/* precision */
			curcol->column_scale = tds_get_byte(tds);	/* scale */
		}

		/* discard Locale */
		tds_get_n(tds, NULL, tds_get_byte(tds));

		tds_add_row_column_size(info, curcol);

		tdsdump_log(TDS_DBG_INFO1, "%L elem %d:\n", col);
		tdsdump_log(TDS_DBG_INFO1, "%L\tcolumn_name=[%s]\n", curcol->column_name);
		tdsdump_log(TDS_DBG_INFO1, "%L\tflags=%x utype=%d type=%d varint=%d\n",
			    curcol->column_flags, curcol->column_usertype, curcol->column_type, curcol->column_varint_size);
		tdsdump_log(TDS_DBG_INFO1, "%L\tcolsize=%d prec=%d scale=%d\n",
			    curcol->column_size, curcol->column_prec, curcol->column_scale);
	}

	if ((info->current_row = tds_alloc_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

/**
 * tds_get_token_size() returns the size of a fixed length token
 * used by tds_process_cancel() to determine how to read past a token
 */
int
tds_get_token_size(int marker)
{
	/* TODO finish */
	switch (marker) {
	case TDS_DONE_TOKEN:
	case TDS_DONEPROC_TOKEN:
	case TDS_DONEINPROC_TOKEN:
		return 8;
	case TDS_RETURNSTATUS_TOKEN:
		return 4;
	case TDS_PROCID_TOKEN:
		return 8;
	default:
		return 0;
	}
}

void
tds_swap_datatype(int coltype, unsigned char *buf)
{
	TDS_NUMERIC *num;

	switch (coltype) {
	case SYBINT2:
		tds_swap_bytes(buf, 2);
		break;
	case SYBINT4:
	case SYBMONEY4:
	case SYBREAL:
		tds_swap_bytes(buf, 4);
		break;
	case SYBINT8:
	case SYBFLT8:
		tds_swap_bytes(buf, 8);
		break;
	case SYBMONEY:
	case SYBDATETIME:
		tds_swap_bytes(buf, 4);
		tds_swap_bytes(&buf[4], 4);
		break;
	case SYBDATETIME4:
		tds_swap_bytes(buf, 2);
		tds_swap_bytes(&buf[2], 2);
		break;
		/* should we place numeric conversion in another place ??
		 * this is not used for big/little-endian conversion... */
	case SYBNUMERIC:
	case SYBDECIMAL:
		num = (TDS_NUMERIC *) buf;
		/* swap the sign */
		num->array[0] = (num->array[0] == 0) ? 1 : 0;
		/* swap the data */
		tds_swap_bytes(&(num->array[1]), tds_numeric_bytes_per_prec[num->precision] - 1);
		break;
	case SYBUNIQUE:
		tds_swap_bytes(buf, 4);
		tds_swap_bytes(&buf[4], 2);
		tds_swap_bytes(&buf[6], 2);
		break;
	}
}

/**
 * tds5_get_varint_size5() returns the size of a variable length integer
 * returned in a TDS 5.1 result string
 */
/* TODO can we use tds_get_varint_size ?? */
static int
tds5_get_varint_size(int datatype)
{
	switch (datatype) {
	case SYBTEXT:
	case SYBNTEXT:
	case SYBIMAGE:
	case SYBVARIANT:
		return 4;

	case SYBLONGBINARY:
	case XSYBCHAR:
		return 5;	/* Special case */

	case SYBVOID:
	case SYBINT1:
	case SYBBIT:
	case SYBINT2:
	case SYBINT4:
	case SYBINT8:
	case SYBDATETIME4:
	case SYBREAL:
	case SYBMONEY:
	case SYBDATETIME:
	case SYBFLT8:
	case SYBMONEY4:
	case SYBSINT1:
	case SYBUINT2:
	case SYBUINT4:
	case SYBUINT8:
		return 0;

	case XSYBNVARCHAR:
	case XSYBVARCHAR:
	case XSYBBINARY:
	case XSYBVARBINARY:
		return 2;

	default:
		return 1;
	}
}

/**
 * tds_process_compute_names() processes compute result sets.  
 */
static int
tds_process_compute_names(TDSSOCKET * tds)
{
	int hdrsize;
	int remainder;
	int num_cols = 0;
	int col;
	int memrc = 0;
	TDS_SMALLINT compute_id = 0;
	TDS_TINYINT namelen;
	TDSCOMPUTEINFO *info;
	TDSCOLINFO *curcol;

	struct namelist
	{
		char name[256];
		int namelen;
		struct namelist *nextptr;
	};

	struct namelist *topptr = NULL;
	struct namelist *curptr = NULL;
	struct namelist *freeptr = NULL;

	hdrsize = tds_get_smallint(tds);
	remainder = hdrsize;
	tdsdump_log(TDS_DBG_INFO1, "%L processing tds5 compute names. remainder = %d\n", remainder);

	/* compute statement id which this relates */
	/* to. You can have more than one compute  */
	/* statement in a SQL statement            */

	compute_id = tds_get_smallint(tds);
	remainder -= 2;

	while (remainder) {
		namelen = tds_get_byte(tds);
		remainder--;
		if (topptr == (struct namelist *) NULL) {
			if ((topptr = (struct namelist *) malloc(sizeof(struct namelist))) == NULL) {
				memrc = -1;
				break;
			}
			curptr = topptr;
			curptr->nextptr = NULL;
		} else {
			if ((curptr->nextptr = (struct namelist *) malloc(sizeof(struct namelist))) == NULL) {
				memrc = -1;
				break;
			}
			curptr = curptr->nextptr;
			curptr->nextptr = NULL;
		}
		if (namelen == 0)
			strcpy(curptr->name, "");
		else {
			namelen = tds_get_string(tds, namelen, curptr->name, sizeof(curptr->name) - 1);
			curptr->name[namelen] = 0;
			remainder -= namelen;
		}
		curptr->namelen = namelen;
		num_cols++;
		tdsdump_log(TDS_DBG_INFO1, "%L processing tds5 compute names. remainder = %d\n", remainder);
	}

	tdsdump_log(TDS_DBG_INFO1, "%L processing tds5 compute names. num_cols = %d\n", num_cols);

	if ((tds->comp_info = tds_alloc_compute_results(&(tds->num_comp_info), tds->comp_info, num_cols, 0)) == NULL)
		memrc = -1;

	tdsdump_log(TDS_DBG_INFO1, "%L processing tds5 compute names. num_comp_info = %d\n", tds->num_comp_info);

	info = tds->comp_info[tds->num_comp_info - 1];
	tds->curr_resinfo = info;

	info->computeid = compute_id;

	curptr = topptr;

	if (memrc == 0) {
		for (col = 0; col < num_cols; col++) {
			curcol = info->columns[col];

			assert(strlen(curcol->column_name) == curcol->column_namelen);
			memcpy(curcol->column_name, curptr->name, curptr->namelen + 1);
			curcol->column_namelen = curptr->namelen;

			freeptr = curptr;
			curptr = curptr->nextptr;
			free(freeptr);
		}
		return TDS_SUCCEED;
	} else {
		while (curptr != NULL) {
			freeptr = curptr;
			curptr = curptr->nextptr;
			free(freeptr);
		}
		return TDS_FAIL;
	}
}

/**
 * tds7_process_compute_result() processes compute result sets for TDS 7/8.
 * They is are very  similar to normal result sets.
 */
static int
tds7_process_compute_result(TDSSOCKET * tds)
{
	int col, num_cols;
	TDS_TINYINT by_cols;
	TDS_TINYINT *cur_by_col;
	TDS_SMALLINT compute_id;
	TDSCOLINFO *curcol;
	TDSCOMPUTEINFO *info;

	/* number of compute columns returned - so */
	/* COMPUTE SUM(x), AVG(x)... would return  */
	/* num_cols = 2                            */

	num_cols = tds_get_smallint(tds);

	tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. num_cols = %d\n", num_cols);

	/* compute statement id which this relates */
	/* to. You can have more than one compute  */
	/* statement in a SQL statement            */

	compute_id = tds_get_smallint(tds);

	tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. compute_id = %d\n", compute_id);
	/* number of "by" columns in compute - so  */
	/* COMPUTE SUM(x) BY a, b, c would return  */
	/* by_cols = 3                             */

	by_cols = tds_get_byte(tds);
	tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. by_cols = %d\n", by_cols);

	if ((tds->comp_info = tds_alloc_compute_results(&(tds->num_comp_info), tds->comp_info, num_cols, by_cols)) == NULL)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. num_comp_info = %d\n", tds->num_comp_info);

	info = tds->comp_info[tds->num_comp_info - 1];
	tds->curr_resinfo = info;

	tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. point 0\n");

	info->computeid = compute_id;

	/* the by columns are a list of the column */
	/* numbers in the select statement         */

	cur_by_col = info->bycolumns;
	for (col = 0; col < by_cols; col++) {
		*cur_by_col = tds_get_smallint(tds);
		cur_by_col++;
	}
	tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. point 1\n");

	for (col = 0; col < num_cols; col++) {
		tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. point 2\n");
		curcol = info->columns[col];

		curcol->column_operator = tds_get_byte(tds);
		curcol->column_operand = tds_get_smallint(tds);

		tds7_get_data_info(tds, curcol);

		if (!curcol->column_namelen) {
			strcpy(curcol->column_name, tds_pr_op(curcol->column_operator));
			curcol->column_namelen = strlen(curcol->column_name);
		}

		tds_add_row_column_size(info, curcol);
	}

	/* all done now allocate a row for tds_process_row to use */
	tdsdump_log(TDS_DBG_INFO1, "%L processing tds7 compute result. point 5 \n");
	if ((info->current_row = tds_alloc_compute_row(info)) != NULL)
		return TDS_SUCCEED;
	else
		return TDS_FAIL;
}

static int 
tds_process_cursor_tokens(TDSSOCKET * tds)
{
	TDS_SMALLINT hdrsize;
	TDS_INT rowcount;
	TDS_INT cursor_id;
	TDS_TINYINT namelen;
	char name[30];	
	unsigned char cursor_cmd;
	TDS_SMALLINT cursor_status;
	
	hdrsize  = tds_get_smallint(tds);
	cursor_id = tds_get_int(tds);
	hdrsize  -= sizeof(TDS_INT);
	if (cursor_id == 0){
		namelen = (int)tds_get_byte(tds);
		hdrsize -= 1;
		tds_get_n(tds, name, namelen);
		hdrsize -= namelen;
	}
	cursor_cmd    = tds_get_byte(tds);
	cursor_status = tds_get_smallint(tds);
	hdrsize -= 3;

	if (hdrsize == sizeof(TDS_INT))
		rowcount = tds_get_int(tds); 

	if (tds->cursor) {
		tds->cursor->cursor_id = cursor_id;
	}

	return TDS_SUCCEED;
}


int
tds5_send_optioncmd(TDSSOCKET * tds, TDS_OPTION_CMD tds_command, TDS_OPTION tds_option, TDS_OPTION_ARG * ptds_argument,
		    TDS_INT * ptds_argsize)
{
	static const TDS_TINYINT token = TDS_OPTIONCMD_TOKEN;
	TDS_TINYINT expected_acknowledgement = 0;
	int marker, status;

	TDS_TINYINT command = tds_command;
	TDS_TINYINT option = tds_option;
	TDS_TINYINT argsize = (*ptds_argsize == TDS_NULLTERM) ? 1 + strlen(ptds_argument->c) : *ptds_argsize;

	TDS_SMALLINT length = sizeof(command) + sizeof(option) + sizeof(argsize) + argsize;

	tdsdump_log(TDS_DBG_INFO1, "%L entering %s::tds_send_optioncmd() \n", __FILE__);

	assert(IS_TDS50(tds));
	assert(ptds_argument);

	tds_put_tinyint(tds, token);
	tds_put_smallint(tds, length);
	tds_put_tinyint(tds, command);
	tds_put_tinyint(tds, option);
	tds_put_tinyint(tds, argsize);

	switch (*ptds_argsize) {
	case 1:
		tds_put_tinyint(tds, ptds_argument->ti);
		break;
	case 4:
		tds_put_int(tds, ptds_argument->i);
		break;
	case TDS_NULLTERM:
		tds_put_string(tds, ptds_argument->c, argsize);
		break;
	default:
		tdsdump_log(TDS_DBG_INFO1, "%L tds_send_optioncmd: failed: argsize is %d.\n", *ptds_argsize);
		return -1;
	}

	tds_flush_packet(tds);

	/* TODO: read the server's response.  Don't use this function yet. */

	switch (command) {
	case TDS_OPT_SET:
	case TDS_OPT_DEFAULT:
		expected_acknowledgement = TDS_DONE_TOKEN;
		break;
	case TDS_OPT_LIST:
		expected_acknowledgement = TDS_OPTIONCMD_TOKEN;	/* with TDS_OPT_INFO */
		break;
	}
	while ((marker = tds_get_byte(tds)) != expected_acknowledgement) {
		tds_process_default_tokens(tds, marker);
	}

	if (marker == TDS_DONE_TOKEN) {
		tds_process_end(tds, marker, &status);
		return (TDS_DONE_FINAL == (status | TDS_DONE_FINAL)) ? TDS_SUCCEED : TDS_FAIL;
	}

	length = tds_get_smallint(tds);
	command = tds_get_byte(tds);
	option = tds_get_byte(tds);
	argsize = tds_get_byte(tds);

	if (argsize > *ptds_argsize) {
		/* return oversize length to caller, copying only as many bytes as caller provided. */
		TDS_INT was = *ptds_argsize;

		*ptds_argsize = argsize;
		argsize = was;
	}

	switch (argsize) {
	case 0:
		break;
	case 1:
		ptds_argument->ti = tds_get_byte(tds);
		break;
	case 4:
		ptds_argument->i = tds_get_int(tds);
		break;
	default:
		/* FIXME not null terminated and size not saved */
		/* FIXME do not take into account conversion */
		tds_get_string(tds, argsize, ptds_argument->c, argsize);
		break;
	}


	while ((marker = tds_get_byte(tds)) != TDS_DONE_TOKEN) {
		tds_process_default_tokens(tds, marker);
	}

	tds_process_end(tds, marker, &status);
	return (TDS_DONE_FINAL == (status | TDS_DONE_FINAL)) ? TDS_SUCCEED : TDS_FAIL;

}

static const char *
tds_pr_op(int op)
{
#define TYPE(con, s) case con: return s; break
	switch (op) {
		TYPE(SYBAOPAVG, "avg");
		TYPE(SYBAOPAVGU, "avg");
		TYPE(SYBAOPCNT, "count");
		TYPE(SYBAOPCNTU, "count");
		TYPE(SYBAOPMAX, "max");
		TYPE(SYBAOPMIN, "min");
		TYPE(SYBAOPSUM, "sum");
		TYPE(SYBAOPSUMU, "sum");
		TYPE(SYBAOPCHECKSUM_AGG, "checksum_agg");
		TYPE(SYBAOPCNT_BIG, "count");
		TYPE(SYBAOPSTDEV, "stdevp");
		TYPE(SYBAOPSTDEVP, "stdevp");
		TYPE(SYBAOPVAR, "var");
		TYPE(SYBAOPVARP, "varp");
	default:
		break;
	}
	return "";
#undef TYPE
}

const char *
tds_prtype(int token)
{
#define TYPE(con, s) case con: return s; break
	switch (token) {
		TYPE(SYBAOPAVG, "avg");
		TYPE(SYBAOPCNT, "count");
		TYPE(SYBAOPMAX, "max");
		TYPE(SYBAOPMIN, "min");
		TYPE(SYBAOPSUM, "sum");

		TYPE(SYBBINARY, "binary");
		TYPE(SYBBIT, "bit");
		TYPE(SYBBITN, "bit-null");
		TYPE(SYBCHAR, "char");
		TYPE(SYBDATETIME4, "smalldatetime");
		TYPE(SYBDATETIME, "datetime");
		TYPE(SYBDATETIMN, "datetime-null");
		TYPE(SYBDECIMAL, "decimal");
		TYPE(SYBFLT8, "float");
		TYPE(SYBFLTN, "float-null");
		TYPE(SYBIMAGE, "image");
		TYPE(SYBINT1, "tinyint");
		TYPE(SYBINT2, "smallint");
		TYPE(SYBINT4, "int");
		TYPE(SYBINT8, "long long");
		TYPE(SYBINTN, "integer-null");
		TYPE(SYBMONEY4, "smallmoney");
		TYPE(SYBMONEY, "money");
		TYPE(SYBMONEYN, "money-null");
		TYPE(SYBNTEXT, "UCS-2 text");
		TYPE(SYBNVARCHAR, "UCS-2 varchar");
		TYPE(SYBNUMERIC, "numeric");
		TYPE(SYBREAL, "real");
		TYPE(SYBTEXT, "text");
		TYPE(SYBUNIQUE, "uniqueidentifier");
		TYPE(SYBVARBINARY, "varbinary");
		TYPE(SYBVARCHAR, "varchar");
		TYPE(SYBVARIANT, "variant");
		TYPE(SYBVOID, "void");
		TYPE(XSYBBINARY, "xbinary");
		TYPE(XSYBCHAR, "xchar");
		TYPE(XSYBNCHAR, "x UCS-2 char");
		TYPE(XSYBNVARCHAR, "x UCS-2 varchar");
		TYPE(XSYBVARBINARY, "xvarbinary");
		TYPE(XSYBVARCHAR, "xvarchar");
	default:
		break;
	}
	return "";
#undef TYPE
}

/** \@} */

static const char *
_tds_token_name(unsigned char marker)
{
	switch (marker) {

	case 0x20:
		return "TDS5_PARAMFMT2";
	case 0x22:
		return "ORDERBY2";
	case 0x61:
		return "ROWFMT2";
	case 0x71:
		return "LOGOUT";
	case 0x79:
		return "RETURNSTATUS";
	case 0x7C:
		return "PROCID";
	case 0x81:
		return "TDS7_RESULT";
	case 0x88:
		return "TDS7_COMPUTE_RESULT";
	case 0xA0:
		return "COLNAME";
	case 0xA1:
		return "COLFMT";
	case 0xA3:
		return "DYNAMIC2";
	case 0xA4:
		return "TABNAME";
	case 0xA5:
		return "COLINFO";
	case 0xA7:
		return "COMPUTE_NAMES";
	case 0xA8:
		return "COMPUTE_RESULT";
	case 0xA9:
		return "ORDERBY";
	case 0xAA:
		return "ERROR";
	case 0xAB:
		return "INFO";
	case 0xAC:
		return "PARAM";
	case 0xAD:
		return "LOGINACK";
	case 0xAE:
		return "CONTROL";
	case 0xD1:
		return "ROW";
	case 0xD3:
		return "CMP_ROW";
	case 0xD7:
		return "TDS5_PARAMS";
	case 0xE2:
		return "CAPABILITY";
	case 0xE3:
		return "ENVCHANGE";
	case 0xE5:
		return "EED";
	case 0xE6:
		return "DBRPC";
	case 0xE7:
		return "TDS5_DYNAMIC";
	case 0xEC:
		return "TDS5_PARAMFMT";
	case 0xED:
		return "AUTH";
	case 0xEE:
		return "RESULT";
	case 0xFD:
		return "DONE";
	case 0xFE:
		return "DONEPROC";
	case 0xFF:
		return "DONEINPROC";

	default:
		break;
	}

	return "";
}

/** 
 * Adjust column size according to client's encoding 
 */
static void
adjust_character_column_size(const TDSSOCKET * tds, TDSCOLINFO * curcol)
{
	/* FIXME: and sybase ?? and single char to utf8 ??? */
	if (is_unicode_type(curcol->on_server.column_type))
		curcol->iconv_info = tds->iconv_info[client2ucs2];

	if (!curcol->iconv_info && IS_TDS7_PLUS(tds))
		curcol->iconv_info = tds->iconv_info[client2server_chardata];

	if (curcol->iconv_info) {
		curcol->on_server.column_size = curcol->column_size;
		curcol->column_size = determine_adjusted_size(curcol->iconv_info, curcol->column_size);
	}
}

/** 
 * Allow for maximum possible size of converted data, 
 * while being careful about integer division truncation. 
 * All character data pass through iconv.  It doesn't matter if the server side 
 * is Unicode or not; even Latin1 text need conversion if,
 * for example, the client is UTF-8.  
 */
static int
determine_adjusted_size(const TDSICONVINFO * iconv_info, int size)
{
	if (!iconv_info)
		return size;

	size *= iconv_info->client_charset.max_bytes_per_char;
	if (size % iconv_info->server_charset.min_bytes_per_char)
		size += iconv_info->server_charset.min_bytes_per_char;
	size /= iconv_info->server_charset.min_bytes_per_char;

	return size;
}
