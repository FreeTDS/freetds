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
#endif

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#include "tdsconvert.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include <assert.h>

static char software_version[] = "$Id: query.c,v 1.87 2003-04-30 18:51:53 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void tds_put_params(TDSSOCKET * tds, TDSPARAMINFO * info, int flags);
static void tds7_put_query_params(TDSSOCKET * tds, const char *query, const char *param_definition);
static int tds_put_data_info(TDSSOCKET * tds, TDSCOLINFO * curcol, int flags);
static int tds_put_data(TDSSOCKET * tds, TDSCOLINFO * curcol, unsigned char *current_row, int i);
static char *tds_build_params_definition(TDSSOCKET * tds, TDSPARAMINFO * params, int *out_len);

#define TDS_PUT_DATA_USE_NAME 1

/* All manner of client to server submittal functions */

/**
 * \ingroup libtds
 * \defgroup query Query
 * Function to handle query.
 */

/**
 * \addtogroup query
 *  \@{ 
 */

/**
 * tds_submit_query() sends a language string to the database server for
 * processing.  TDS 4.2 is a plain text message with a packet type of 0x01,
 * TDS 7.0 is a unicode string with packet type 0x01, and TDS 5.0 uses a 
 * TDS_LANGUAGE_TOKEN to encapsulate the query and a packet type of 0x0f.
 * @param query language query to submit
 * @return TDS_FAIL or TDS_SUCCEED
 */
int
tds_submit_query(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params)
{
	TDSCOLINFO *param;
	int query_len, i;

	if (!query)
		return TDS_FAIL;

	/* Jeff's hack to handle long query timeouts */
	tds->queryStarttime = time(NULL);

	if (tds->state != TDS_IDLE) {
		tdsdump_log(TDS_DBG_ERROR, "tds_submit_query(): state is PENDING\n");
		tds_client_msg(tds->tds_ctx, tds, 20019, 7, 0, 1,
			       "Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}

	tds_free_all_results(tds);

	tds->rows_affected = TDS_NO_COUNT;
	tds->state = TDS_QUERYING;

	query_len = strlen(query);
	if (IS_TDS50(tds)) {
		tds->out_flag = 0x0F;
		tds_put_byte(tds, TDS_LANGUAGE_TOKEN);
		/* FIXME ICONV use converted size, not input size and convert string */
		tds_put_int(tds, query_len + 1);
		tds_put_byte(tds, params ? 1 : 0);	/* 1 if there are params, 0 otherwise */
		tds_put_n(tds, query, query_len);
		if (params) {
			/* add on parameters */
			tds_put_params(tds, params, params->columns[0]->column_name[0] ? TDS_PUT_DATA_USE_NAME : 0);
		}
	} else if (!IS_TDS7_PLUS(tds) || !params || !params->num_cols) {
		tds->out_flag = 0x01;
		tds_put_string(tds, query, query_len);
	} else {
		int definition_len;
		char *param_definition = tds_build_params_definition(tds, params, &definition_len);

		/* out of memory or invalid parameters ?? */
		if (!param_definition)
			return TDS_FAIL;

		tds->out_flag = 3;	/* RPC */
		/* procedure name */
		tds_put_smallint(tds, 13);
		tds_put_n(tds, "s\0p\0_\0e\0x\0e\0c\0u\0t\0e\0s\0q\0l", 26);
		tds_put_smallint(tds, 0);

		/* string with sql statement */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 0);
		tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
		if (IS_TDS80(tds))
			tds_put_n(tds, tds->collation, 5);
		/* FIXME ICONV use converted size */
		tds_put_int(tds, query_len * 2);
		tds_put_int(tds, query_len * 2);
		tds_put_string(tds, query, query_len);

		/* params definitions */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 0);
		tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
		if (IS_TDS80(tds))
			tds_put_n(tds, tds->collation, 5);
		/* FIXME ICONV someone should change results from tds_build_params_definition to ucs2 and use provided length */
		tds_put_int(tds, definition_len * 2);
		tds_put_int(tds, definition_len * 2);
		tds_put_string(tds, param_definition, definition_len);

		for (i = 0; i < params->num_cols; i++) {
			param = params->columns[i];
			tds_put_data_info(tds, param, 0);
			tds_put_data(tds, param, params->current_row, i);
		}

		return tds_flush_packet(tds);
	}
	return tds_flush_packet(tds);
}

int
tds_submit_queryf(TDSSOCKET * tds, const char *queryf, ...)
{
	va_list ap;
	char *query = NULL;
	int rc = TDS_FAIL;

	va_start(ap, queryf);
	if (vasprintf(&query, queryf, ap) >= 0) {
		rc = tds_submit_query(tds, query, NULL);
		free(query);
	}
	va_end(ap);
	return rc;
}


/**
 * Skip quoting string (like 'sfsf', "dflkdj" or [dfkjd])
 * @param s pointer to first quoting character (should be '," or [)
 * @return character after quoting
 */
static const char *
tds_skip_quoted(const char *s)
{
	const char *p = s;
	char quote = (*s == '[') ? ']' : *s;

	for (; *++p;) {
		if (*p == quote) {
			if (*++p != quote)
				return p;
		}
	}
	return p;
}

/**
 * Get position of next placeholders
 * @param start pointer to part of query to search
 * @return next placaholders or NULL if not found
 */
const char *
tds_next_placeholders(const char *start)
{
	const char *p = start;

	if (!p)
		return NULL;

	for (;;) {
		switch (*p) {
		case '\0':
			return NULL;
		case '\'':
		case '\"':
		case '[':
			p = tds_skip_quoted(p);
			break;
		case '?':
			return p;
		default:
			++p;
			break;
		}
	}
}

/**
 * Count the number of placeholders in query
 */
int
tds_count_placeholders(const char *query)
{
	const char *p = query - 1;
	int count = 0;

	for (;; ++count) {
		if (!(p = tds_next_placeholders(p + 1)))
			return count;
	}
}

/**
 * Return declaration for column (like "varchar(20)")
 * \param curcol column
 * \param out    buffer to hold declaration
 */
static void
tds_get_column_declaration(TDSSOCKET * tds, TDSCOLINFO * curcol, char *out)
{
	const char *fmt = NULL;

	switch (tds_get_conversion_type(curcol->column_type, curcol->column_size)) {
	case XSYBCHAR:
	case SYBCHAR:
		fmt = "CHAR(%d)";
		break;
	case SYBVARCHAR:
	case XSYBVARCHAR:
		fmt = "VARCHAR(%d)";
		break;
	case SYBINT1:
		fmt = "TINYINT";
		break;
	case SYBINT2:
		fmt = "SMALLINT";
		break;
	case SYBINT4:
		fmt = "INT";
		break;
	case SYBINT8:
		/* TODO even for Sybase ?? */
		fmt = "BIGINT";
		break;
	case SYBFLT8:
		fmt = "FLOAT";
		break;
	case SYBDATETIME:
		fmt = "DATETIME";
		break;
	case SYBBIT:
		fmt = "BIT";
		break;
	case SYBTEXT:
		fmt = "TEXT";
		break;
	case SYBLONGBINARY:	/* TODO correct ?? */
	case SYBIMAGE:
		fmt = "IMAGE";
		break;
	case SYBMONEY4:
		fmt = "SMALLMONEY";
		break;
	case SYBMONEY:
		fmt = "MONEY";
		break;
	case SYBDATETIME4:
		fmt = "SMALLDATETIME";
		break;
	case SYBREAL:
		fmt = "REAL";
		break;
	case SYBBINARY:
	case XSYBBINARY:
		fmt = "BINARY";
		break;
	case SYBVARBINARY:
	case XSYBVARBINARY:
		fmt = "VARBINARY(%d)";
		break;
	case SYBNUMERIC:
		fmt = "NUMERIC(%d,%d)";
		goto numeric_decimal;
	case SYBDECIMAL:
		fmt = "DECIMAL(%d,%d)";
	      numeric_decimal:
		sprintf(out, fmt, curcol->column_prec, curcol->column_scale);
		return;
		break;
		/* nullable types should not occur here... */
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	case SYBBITN:
	case SYBINTN:
		assert(0);
		/* TODO... */
	case SYBNTEXT:
	case SYBVOID:
	case SYBNVARCHAR:
	case XSYBNVARCHAR:
	case XSYBNCHAR:
	case SYBSINT1:
	case SYBUINT2:
	case SYBUINT4:
	case SYBUINT8:
	case SYBUNIQUE:
	case SYBVARIANT:
		out[0] = 0;
		return;
		break;
	}

	/* fill out */
	sprintf(out, fmt, curcol->column_size);
}

/**
 * Return string with parameters definition
 * \param paramss parameters to build declaration
 * \param out_len length in buffer
 * \return allocated and filled string or NULL on failure
 */
static char *
tds_build_params_definition(TDSSOCKET * tds, TDSPARAMINFO * params, int *out_len)
{
	int size = 512;

	/* TODO check out of memory */
	char *param_str = (char *) malloc(512);
	char *p;
	int l = 0, i;

	/* FIXME ICONV return ucs2, see above */

	assert(IS_TDS7_PLUS(tds));

	if (!param_str)
		return NULL;
	param_str[0] = 0;
	for (i = 0; i < params->num_cols; ++i) {
		if (l > 0)
			param_str[l++] = ',';

		/* FIXME why ??? */
		params->columns[i]->column_namelen = strlen(params->columns[i]->column_name);

		/* realloc on insufficient space */
		while ((l + 24 + params->columns[i]->column_namelen) > size) {
			p = (char *) realloc(param_str, size += 512);
			if (!p) {
				free(param_str);
				return NULL;
			}
			param_str = p;
		}

		/* FIXME ICONV this part of buffer can be not-ascii compatible, use all ucs2... */
		memcpy(param_str + l, params->columns[i]->column_name, params->columns[i]->column_namelen);
		l += params->columns[i]->column_namelen;
		param_str[l++] = ' ';

		/* append this parameter */
		/* FIXME ICONV convert to ucs2... */
		tds_get_column_declaration(tds, params->columns[i], param_str + l);
		if (!param_str[l]) {
			free(param_str);
			return NULL;
		}
		l += strlen(param_str + l);
	}
	*out_len = l;
	return param_str;
}

/**
 * Output params types and query (required by sp_prepare/sp_executesql/sp_prepexec)
 */
static void
tds7_put_query_params(TDSSOCKET * tds, const char *query, const char *param_definition)
{
	int len, i, n;
	const char *s, *e;
	char buf[24];

	assert(IS_TDS7_PLUS(tds));

	/* TODO placeholder should be same number as parameters in definition ??? */

	/* string with parameters types */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
	if (IS_TDS80(tds))
		tds_put_n(tds, tds->collation, 5);
	/* for now we use all "@PX varchar(80)," for parameters (same behavior of mssql2k) */
	n = tds_count_placeholders(query);
	len = n * 16 - 1;
	/* adjust for the length of X */
	for (i = 10; i <= n; i *= 10) {
		len += n - i + 1;
	}
	if (!param_definition) {
		/* TODO put this code in caller and pass param_definition */
		tds_put_int(tds, len * 2);
		tds_put_int(tds, len * 2);
		for (i = 1; i <= n; ++i) {
			sprintf(buf, "%s@P%d varchar(80)", (i == 1 ? "" : ","), i);
			tds_put_string(tds, buf, -1);
		}
	} else {
		/* FIXME ICONV just to add some incompatibility with charset... see above */
		i = strlen(param_definition);
		tds_put_int(tds, i * 2);
		tds_put_int(tds, i * 2);
		tds_put_string(tds, param_definition, i);
	}

	/* string with sql statement */
	/* replace placeholders with dummy parametes */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
	if (IS_TDS80(tds))
		tds_put_n(tds, tds->collation, 5);
	len = (len + 1 - 14 * n) + strlen(query);
	/* FIXME ICONV use converted size. Perhaps we should construct entire string ? */
	tds_put_int(tds, len * 2);
	tds_put_int(tds, len * 2);
	s = query;
	/* TODO do a test with "...?" and "...?)" */
	for (i = 1;; ++i) {
		e = tds_next_placeholders(s);
		tds_put_string(tds, s, e ? e - s : strlen(s));
		if (!e)
			break;
		sprintf(buf, "@P%d", i);
		tds_put_string(tds, buf, -1);
		s = e + 1;
	}
}

/**
 * tds_submit_prepare() creates a temporary stored procedure in the server.
 * Currently works with TDS 5.0 and TDS7+
 * @param query language query with given placeholders (?)
 * @param id string to identify the dynamic query. Pass NULL for automatic generation.
 * @param dyn_out will receive allocated TDSDYNAMIC*. Any older allocated dynamic won't be freed, Can be NULL.
 * @return TDS_FAIL or TDS_SUCCEED
 */
/* TODO parse all results ?? */
int
tds_submit_prepare(TDSSOCKET * tds, const char *query, const char *id, TDSDYNAMIC ** dyn_out, TDSPARAMINFO * params)
{
	int id_len, query_len;
	TDSDYNAMIC *dyn;

	if (!query)
		return TDS_FAIL;

	if (!IS_TDS50(tds) && !IS_TDS7_PLUS(tds)) {
		tdsdump_log(TDS_DBG_ERROR, "Dynamic placeholders only supported under TDS 5.0 and TDS 7.0+\n");
		return TDS_FAIL;
	}
	if (tds->state != TDS_IDLE) {
		tds_client_msg(tds->tds_ctx, tds, 20019, 7, 0, 1,
			       "Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}
	tds_free_all_results(tds);

	/* allocate a structure for this thing */
	if (!id) {
		char *tmp_id = NULL;

		if (tds_get_dynid(tds, &tmp_id) == TDS_FAIL)
			return TDS_FAIL;
		dyn = tds_alloc_dynamic(tds, tmp_id);
		TDS_ZERO_FREE(tmp_id);
		if (!dyn)
			return TDS_FAIL;
	} else {
		dyn = tds_alloc_dynamic(tds, id);
	}
	if (!dyn)
		return TDS_FAIL;

	tds->cur_dyn = dyn;

	if (dyn_out)
		*dyn_out = dyn;

	tds->rows_affected = TDS_NO_COUNT;
	tds->state = TDS_QUERYING;
	query_len = strlen(query);

	if (IS_TDS7_PLUS(tds)) {
		int definition_len, i;
		char *param_definition = NULL;

		if (params) {
			/* place dummy parameters */
			for (i = 0; i < params->num_cols; ++i) {
				sprintf(params->columns[i]->column_name, "@P%d", i + 1);
				params->columns[i]->column_namelen = strlen(params->columns[i]->column_name);
			}
			param_definition = tds_build_params_definition(tds, params, &definition_len);
			if (!param_definition)
				return TDS_FAIL;
		}

		tds->out_flag = 3;	/* RPC */
		/* procedure name */
		tds_put_smallint(tds, 10);
		tds_put_n(tds, "s\0p\0_\0p\0r\0e\0p\0a\0r\0e", 20);
		tds_put_smallint(tds, 0);

		/* return param handle (int) */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 1);	/* result */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 0);

		tds7_put_query_params(tds, query, param_definition);

		/* 1 param ?? why ? flags ?? */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 0);
		tds_put_byte(tds, SYBINT4);
		tds_put_int(tds, 1);

		return tds_flush_packet(tds);
	}

	tds->out_flag = 0x0F;

	id_len = strlen(dyn->id);
	tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
	tds_put_smallint(tds, query_len + id_len * 2 + 21);
	tds_put_byte(tds, 0x01);
	tds_put_byte(tds, 0x00);
	tds_put_byte(tds, id_len);
	tds_put_n(tds, dyn->id, id_len);
	/* FIXME ICONV use converted size */
	/* TODO how to pass parameters type? like store procedures ? */
	tds_put_smallint(tds, query_len + id_len + 16);
	tds_put_n(tds, "create proc ", 12);
	tds_put_n(tds, dyn->id, id_len);
	tds_put_n(tds, " as ", 4);
	tds_put_n(tds, query, query_len);

	return tds_flush_packet(tds);
}


/**
 * Put data information to wire
 * @param curcol column where to store information
 * @param flags  bit flags on how to send data (use TDS_PUT_DATA_USE_NAME for use name information)
 * @return TDS_SUCCEED or TDS_FAIL
 */
static int
tds_put_data_info(TDSSOCKET * tds, TDSCOLINFO * curcol, int flags)
{
	int len;

	if (flags & TDS_PUT_DATA_USE_NAME) {
		/* TODO use column_namelen ?? */
		len = strlen(curcol->column_name);
		tdsdump_log(TDS_DBG_ERROR, "%L tds_put_data_info putting param_name \n");
		/* FIXME ICONV use converted size */
		tds_put_byte(tds, len);	/* param name len */
		tds_put_string(tds, curcol->column_name, len);
	} else {
		tds_put_byte(tds, 0x00);	/* param name len */
	}
	/* TODO support other flags (use defaul null/no metadata)
	 * bit 1 (2 as flag) in TDS7+ is "default value" bit 
	 * (what's the meaning of "default value" ?) */

	tdsdump_log(TDS_DBG_ERROR, "%L tds_put_data_info putting status \n");
	tds_put_byte(tds, curcol->column_output);	/* status (input) */
	if (!IS_TDS7_PLUS(tds))
		tds_put_int(tds, curcol->column_usertype);	/* usertype */
	tds_put_byte(tds, curcol->on_server.column_type);

	switch (curcol->column_varint_size) {
	case 0:
		break;
	case 1:
		tds_put_byte(tds, curcol->column_size);
		break;
	case 2:
		tds_put_smallint(tds, curcol->column_size);
		break;
	case 4:
		tds_put_int(tds, curcol->column_size);
		break;
	}
	if (is_numeric_type(curcol->column_type)) {
		tds_put_byte(tds, curcol->column_prec);
		tds_put_byte(tds, curcol->column_scale);
	}

	/* TDS8 output collate information */
	if (IS_TDS80(tds) && is_collate_type(curcol->column_type))
		tds_put_n(tds, tds->collation, 5);

	/* TODO needed in TDS4.2 ?? now is called only is TDS >= 5 */
	if (!IS_TDS7_PLUS(tds)) {

		tdsdump_log(TDS_DBG_ERROR, "%L HERE! \n");
		tds_put_byte(tds, 0x00);	/* locale info length */
	}
	return TDS_SUCCEED;
}

/**
 * Calc information length in bytes (useful for calculating full packet length)
 * @param curcol column where to store information
 * @param flags  bit flags on how to send data (use TDS_PUT_DATA_USE_NAME for use name information)
 * @return TDS_SUCCEED or TDS_FAIL
 */
static int
tds_put_data_info_length(TDSSOCKET * tds, TDSCOLINFO * curcol, int flags)
{
	int len = 8;

#ifdef ENABLE_EXTRA_CHECKS
	if (IS_TDS7_PLUS(tds))
		tdsdump_log(TDS_DBG_ERROR, "%L tds_put_data_info_length called with TDS7+\n");
#endif

	/* FIXME ICONV use converted size */
	if (flags & TDS_PUT_DATA_USE_NAME)
		/* TODO use column_namelen ? */
		len += strlen(curcol->column_name);
	if (is_numeric_type(curcol->column_type))
		len += 2;
	return len + curcol->column_varint_size;
}

/**
 * Write data to wire
 * @param curcol column where store column information
 * @param pointer to row data to store information
 * @param i column position in current_row
 * @return TDS_FAIL on error or TDS_SUCCEED
 */
static int
tds_put_data(TDSSOCKET * tds, TDSCOLINFO * curcol, unsigned char *current_row, int i)
{
	unsigned char *src;
	TDS_NUMERIC *num;
	TDSBLOBINFO *blob_info;
	int colsize;
	int is_null;
	const unsigned char CHARBIN_NULL[] = { 0xff, 0xff };
	const unsigned char GEN_NULL = 0x00;

/* I don't think this is working as tds_set_null is not being called prior to this...
   I can't figure out how where I should call tds_set_null() anyway....

	is_null = tds_get_null(current_row, i);
*/
	colsize = curcol->column_cur_size;
	if (colsize == 0)
		is_null = 1;
	else
		is_null = 0;

	tdsdump_log(TDS_DBG_INFO1, "%L tds_put_data: is_null = %d, colsize = %d\n", is_null, colsize);

	/* FIXME ICONV handle charset conversions for data */

	if (IS_TDS7_PLUS(tds)) {
		src = &(current_row[curcol->column_offset]);
		if (is_null) {

			tdsdump_log(TDS_DBG_INFO1, "%L tds_put_data: null param\n");
			switch (curcol->column_type) {
			case XSYBCHAR:
			case XSYBVARCHAR:
			case XSYBBINARY:
			case XSYBVARBINARY:
			case XSYBNCHAR:
			case XSYBNVARCHAR:
				tdsdump_log(TDS_DBG_INFO1, "%L tds_put_data: putting CHARBIN_NULL\n");
				tds_put_n(tds, CHARBIN_NULL, 2);
				break;
			default:
				tdsdump_log(TDS_DBG_INFO1, "%L tds_put_data: putting GEN_NULL\n");
				tds_put_byte(tds, GEN_NULL);
				break;

			}
		} else {
			tdsdump_log(TDS_DBG_INFO1, "%L tds_put_data: not null param varint_size = %d\n",
				    curcol->column_varint_size);
			switch (curcol->column_varint_size) {
			case 4:	/* Its a BLOB... */
				blob_info = (TDSBLOBINFO *) & (current_row[curcol->column_offset]);
				/* mssql require only size */
				tds_put_int(tds, colsize);
				break;
			case 2:
				tds_put_smallint(tds, colsize);
				break;
			case 1:
				if (is_numeric_type(curcol->column_type))
					colsize = tds_numeric_bytes_per_prec[((TDS_NUMERIC *) src)->precision];
				tds_put_byte(tds, colsize);
				break;
			case 0:
				colsize = tds_get_size_by_type(curcol->column_type);
				break;
			}

			/* put real data */
			if (is_numeric_type(curcol->column_type)) {
				TDS_NUMERIC buf;

				num = (TDS_NUMERIC *) src;
				memcpy(&buf, num, sizeof(buf));
				tdsdump_log(TDS_DBG_INFO1, "%L swapping numeric data...\n");
				tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), (unsigned char *) &buf);
				num = &buf;
				tds_put_n(tds, num->array, colsize);
			} else if (is_blob_type(curcol->column_type)) {
				blob_info = (TDSBLOBINFO *) src;
				/* FIXME ICONV support conversions */
				tds_put_n(tds, blob_info->textvalue, colsize);
			} else {
#ifdef WORDS_BIGENDIAN
				unsigned char buf[64];

				if (tds->emul_little_endian && !is_numeric_type(curcol->column_type) && colsize < 64) {
					tdsdump_log(TDS_DBG_INFO1, "%L swapping coltype %d\n",
						    tds_get_conversion_type(curcol->column_type, colsize));
					memcpy(buf, src, colsize);
					tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), buf);
					src = buf;
				}
#endif
				tds_put_n(tds, src, colsize);
			}
		}
	} else {
		/* put size of data */
		src = &(current_row[curcol->column_offset]);
		switch (curcol->column_varint_size) {
		case 4:	/* Its a BLOB... */
			blob_info = (TDSBLOBINFO *) & (current_row[curcol->column_offset]);
			if (!is_null) {
				tds_put_byte(tds, 16);
				tds_put_n(tds, blob_info->textptr, 16);
				tds_put_n(tds, blob_info->timestamp, 8);
				tds_put_int(tds, colsize);
			} else {
				tds_put_byte(tds, 0);
			}
			break;
		case 2:
			if (!is_null)
				tds_put_smallint(tds, colsize);
			else
				tds_put_smallint(tds, -1);
			break;
		case 1:
			if (!is_null) {
				if (is_numeric_type(curcol->column_type))
					colsize = tds_numeric_bytes_per_prec[((TDS_NUMERIC *) src)->precision];
				tds_put_byte(tds, colsize);
			} else
				tds_put_byte(tds, 0);
			break;
		case 0:
			colsize = tds_get_size_by_type(curcol->column_type);
			break;
		}

		if (is_null)
			return TDS_SUCCEED;

		/* put real data */
		if (is_numeric_type(curcol->column_type)) {
			TDS_NUMERIC buf;

			num = (TDS_NUMERIC *) src;
			if (IS_TDS7_PLUS(tds)) {
				memcpy(&buf, num, sizeof(buf));
				tdsdump_log(TDS_DBG_INFO1, "%L swapping numeric data...\n");
				tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), (unsigned char *) &buf);
				num = &buf;
			}
			tds_put_n(tds, num->array, colsize);
		} else if (is_blob_type(curcol->column_type)) {
			blob_info = (TDSBLOBINFO *) src;
			/* FIXME ICONV handle conversion when needed */
			tds_put_n(tds, blob_info->textvalue, colsize);
		} else {
#ifdef WORDS_BIGENDIAN
			unsigned char buf[64];

			if (tds->emul_little_endian && !is_numeric_type(curcol->column_type) && colsize < 64) {
				tdsdump_log(TDS_DBG_INFO1, "%L swapping coltype %d\n",
					    tds_get_conversion_type(curcol->column_type, colsize));
				memcpy(buf, src, colsize);
				tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), buf);
				src = buf;
			}
#endif
			tds_put_n(tds, src, colsize);
		}
	}
	return TDS_SUCCEED;
}

/**
 * tds_submit_execute() sends a previously prepared dynamic statement to the 
 * server.
 * Currently works with TDS 5.0 or TDS7+
 * @param dyn dynamic proc to execute. Must build from same tds.
 */
int
tds_submit_execute(TDSSOCKET * tds, TDSDYNAMIC * dyn)
{
	TDSCOLINFO *param;
	TDSPARAMINFO *info;
	int id_len;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "%L inside tds_submit_execute()\n");

	if (tds->state != TDS_IDLE) {
		tds_client_msg(tds->tds_ctx, tds, 20019, 7, 0, 1,
			       "Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}

	/* TODO check this code, copied from tds_submit_prepare */
	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;
	tds->state = TDS_QUERYING;

	tds->cur_dyn = dyn;

	if (IS_TDS7_PLUS(tds)) {
		/* RPC on sp_execute */
		tds->out_flag = 3;	/* RPC */
		/* procedure name */
		tds_put_smallint(tds, 10);
		tds_put_n(tds, "s\0p\0_\0e\0x\0e\0c\0u\0t\0e", 20);
		tds_put_smallint(tds, 0);	/* flags */

		/* id of prepared statement */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 0);
		tds_put_byte(tds, SYBINT4);
		tds_put_int(tds, dyn->num_id);

		info = dyn->params;
		for (i = 0; i < info->num_cols; i++) {
			param = info->columns[i];
			tds_put_data_info(tds, param, 0);
			tds_put_data(tds, param, info->current_row, i);
		}

		return tds_flush_packet(tds);
	}

	tds->out_flag = 0x0F;
	/* dynamic id */
	id_len = strlen(dyn->id);

	tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
	tds_put_smallint(tds, id_len + 5);
	tds_put_byte(tds, 0x02);
	tds_put_byte(tds, 0x01);
	tds_put_byte(tds, id_len);
	tds_put_n(tds, dyn->id, id_len);
	tds_put_smallint(tds, 0);

	tds_put_params(tds, dyn->params, 0);

	/* send it */
	return tds_flush_packet(tds);
}

static void
tds_put_params(TDSSOCKET * tds, TDSPARAMINFO * info, int flags)
{
	int i, len;

	/* column descriptions */
	tds_put_byte(tds, TDS5_PARAMFMT_TOKEN);
	/* size */
	len = 2;
	for (i = 0; i < info->num_cols; i++)
		len += tds_put_data_info_length(tds, info->columns[i], flags);
	tds_put_smallint(tds, len);
	/* number of parameters */
	tds_put_smallint(tds, info->num_cols);
	/* column detail for each parameter */
	for (i = 0; i < info->num_cols; i++) {
		/* FIXME add error handling */
		tds_put_data_info(tds, info->columns[i], flags);
	}

	/* row data */
	tds_put_byte(tds, TDS5_PARAMS_TOKEN);
	for (i = 0; i < info->num_cols; i++) {
		tds_put_data(tds, info->columns[i], info->current_row, i);
	}
}

static volatile int inc_num = 1;

/**
 * Get an id for dynamic query based on TDS information
 * @return TDS_FAIL or TDS_SUCCEED
 */
int
tds_get_dynid(TDSSOCKET * tds, char **id)
{
	unsigned long n;
	int i;
	char *p;
	char c;

	inc_num = (inc_num + 1) & 0xffff;
	/* some version of Sybase require length <= 10, so we code id */
	n = (unsigned long) tds;
	if (!(p = (char *) malloc(16)))
		return TDS_FAIL;
	*id = p;
	*p++ = (char) ('a' + (n % 26u));
	n /= 26u;
	for (i = 0; i < 9; ++i) {
		c = (char) ('0' + (n % 36u));
		*p++ = (c < ('0' + 10)) ? c : c + ('a' - '0' - 10);
		/* printf("%d -> %d(%c)\n",n%36u,p[-1],p[-1]); */
		n /= 36u;
		if (i == 4)
			n += 3u * inc_num;
	}
	*p++ = 0;
	return TDS_SUCCEED;
}

/**
 * Send a unprepare request for a prepared query
 * @param tds db connection
 * @param dyn dynamic query
 * @result TDS_SUCCEED or TDS_FAIL
 */
int
tds_submit_unprepare(TDSSOCKET * tds, TDSDYNAMIC * dyn)
{
	int id_len;

	if (!dyn)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_FUNC, "%L inside tds_submit_unprepare() %s\n", dyn->id);

	if (tds->state != TDS_IDLE) {
		tds_client_msg(tds->tds_ctx, tds, 20019, 7, 0, 1,
			       "Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}

	/* TODO check this code, copied from tds_submit_prepare */
	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;
	tds->state = TDS_QUERYING;

	tds->cur_dyn = dyn;

	if (IS_TDS7_PLUS(tds)) {
		/* RPC on sp_execute */
		tds->out_flag = 3;	/* RPC */
		/* procedure name */
		if (IS_TDS80(tds)) {
			/* save some byte for mssql2k */
			/* TODO use similar method even above */
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, 15);
		} else {
			tds_put_smallint(tds, 12);
			tds_put_n(tds, "s\0p\0_\0u\0n\0p\0r\0e\0p\0a\0r\0e", 24);
		}
		tds_put_smallint(tds, 0);	/* flags */

		/* id of prepared statement */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 0);
		tds_put_byte(tds, SYBINT4);
		tds_put_int(tds, dyn->num_id);

		return tds_flush_packet(tds);
	}

	tds->out_flag = 0x0F;
	/* dynamic id */
	id_len = strlen(dyn->id);

	tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
	tds_put_smallint(tds, id_len + 5);
	tds_put_byte(tds, 0x04);
	tds_put_byte(tds, 0x00);
	tds_put_byte(tds, id_len);
	tds_put_n(tds, dyn->id, id_len);
	tds_put_smallint(tds, 0);

	/* send it */
	return tds_flush_packet(tds);
}

/**
 * tds_submit_rpc() call a RPC from server. Output parameters will be stored in tds->param_info
 * @param rpc_name name of RPC
 * @param params   parameters informations. NULL for no parameters
 */
int
tds_submit_rpc(TDSSOCKET * tds, const char *rpc_name, TDSPARAMINFO * params)
{
	TDSCOLINFO *param;
	int rpc_name_len, i;
	int num_params = params ? params->num_cols : 0;

	assert(tds);
	assert(rpc_name);

	if (tds->state != TDS_IDLE) {
		tds_client_msg(tds->tds_ctx, tds, 20019, 7, 0, 1,
			       "Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}

	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;
	tds->state = TDS_QUERYING;

	/* distinguish from dynamic query  */
	tds->cur_dyn = NULL;

	rpc_name_len = strlen(rpc_name);
	if (IS_TDS7_PLUS(tds)) {
		tds->out_flag = 3;	/* RPC */
		/* procedure name */
		/* FIXME ICONV use converted size */
		tds_put_smallint(tds, rpc_name_len);
		tds_put_string(tds, rpc_name, rpc_name_len);
		/* TODO support flags
		 * bit 0 (1 as flag) in TDS7/TDS5 is "recompile"
		 * bit 1 (2 as flag) in TDS7+ is "no metadata" bit 
		 * (I don't know meaning of "no metadata") */
		tds_put_smallint(tds, 0);

		for (i = 0; i < num_params; i++) {
			param = params->columns[i];
			tds_put_data_info(tds, param, TDS_PUT_DATA_USE_NAME);
			tds_put_data(tds, param, params->current_row, i);
		}

		return tds_flush_packet(tds);
	}

	if (IS_TDS50(tds)) {
		tds->out_flag = 0xf;	/* normal */

		/* DBRPC */
		tds_put_byte(tds, TDS_DBRPC_TOKEN);
		/* FIXME ICONV use converted size */
		tds_put_smallint(tds, rpc_name_len + 3);
		tds_put_byte(tds, rpc_name_len);
		tds_put_string(tds, rpc_name, rpc_name_len);
		/* TODO flags */
		tds_put_smallint(tds, num_params ? 2 : 0);

		if (num_params)
			tds_put_params(tds, params, TDS_PUT_DATA_USE_NAME);

		/* send it */
		return tds_flush_packet(tds);
	}

	/* TODO continue, support for TDS4?? */
	return TDS_FAIL;
}

/**
 * tds_send_cancel() sends an empty packet (8 byte header only)
 * tds_process_cancel should be called directly after this.
 */
int
tds_send_cancel(TDSSOCKET * tds)
{
	/* TODO discard any partial packet here */
	/* tds_init_write_buf(tds); */

	tds->out_flag = 0x06;
	return tds_flush_packet(tds);
}

static int
tds_quote(TDSSOCKET * tds, char *buffer, char quoting, const char *id, int len)
{
	int i;
	const char *src, *pend;
	char *dst;

	pend = id + len;

	/* quote */
	src = id;
	if (!buffer) {
		i = 2 + len;
		for (; src != pend; ++src)
			if (*src == quoting)
				++i;
		return i;
	}

	dst = buffer;
	*dst++ = (quoting == ']') ? '[' : quoting;
	for (; src != pend; ++src) {
		if (*src == quoting)
			*dst++ = quoting;
		*dst++ = *src;
	}
	*dst++ = quoting;
	*dst = 0;
	return dst - buffer;
}

/**
 * Quote an id
 * @param buffer buffer to store quoted id. If NULL do not write anything 
 *        (useful to compute quote length)
 * @param id     id to quote
 * @result written chars (not including needed terminator)
 */
int
tds_quote_id(TDSSOCKET * tds, char *buffer, const char *id)
{
	int need_quote;
	int len = strlen(id);

	/* need quote ?? */
	need_quote = (strcspn(id, "\"\' ()[]{}") != len);

	if (!need_quote) {
		if (buffer)
			memcpy(buffer, id, len + 1);
		return len;
	}

	return tds_quote(tds, buffer, TDS_IS_MSSQL(tds) ? ']' : '\"', id, len);
}

/**
 * Quote a string
 * @param buffer buffer to store quoted id. If NULL do not write anything 
 *        (useful to compute quote length)
 * @param str    string to quote (not necessary null-terminated)
 * @param len    length of string (-1 for null terminated)
 * @result written chars (not including needed terminator)
 */
int
tds_quote_string(TDSSOCKET * tds, char *buffer, const char *str, int len)
{
	return tds_quote(tds, buffer, '\'', str, len < 0 ? strlen(str) : len);
}

/* TODO add function to return type suitable for param
 * ie:
 * sybvarchar -> sybvarchar / xsybvarchar
 * sybint4 -> sybintn
 */

/** \@} */
