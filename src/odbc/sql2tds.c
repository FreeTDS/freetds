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

#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#include "tdsodbc.h"
#include "tdsconvert.h"
#include "sql2tds.h"
#include "convert_sql2string.h"
#include "odbc_util.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: sql2tds.c,v 1.8 2003-04-30 08:47:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };


extern const int tds_numeric_bytes_per_prec[];

/**
 * Convert parameters to libtds format
 * return same result of tds_convert
 */
int
sql2tds(TDS_DBC * dbc, struct _sql_param_info *param, TDSPARAMINFO * info, TDSCOLINFO * curcol)
{
	int dest_type, src_type, res;
	CONV_RESULT ores;
	TDSBLOBINFO *blob_info;
	unsigned char *dest;

	/* TODO handle bindings of char like "{d '2002-11-12'}" */
	tdsdump_log(TDS_DBG_INFO2, "%s:%d type=%d\n", __FILE__, __LINE__, param->param_sqltype);

	/* what type to convert ? */
	dest_type = odbc_sql_to_server_type(dbc->tds_socket, param->param_sqltype);
	if (dest_type == TDS_FAIL)
		return TDS_CONVERT_FAIL;
	tdsdump_log(TDS_DBG_INFO2, "%s:%d\n", __FILE__, __LINE__);
	/* TODO what happen for unicode types ?? */
	tds_set_column_type(curcol, dest_type);
	if (curcol->column_varint_size != 0)
		curcol->column_cur_size = curcol->column_size = *param->param_lenbind;
	tdsdump_log(TDS_DBG_INFO2, "%s:%d\n", __FILE__, __LINE__);

	/* allocate given space */
	if (!tds_alloc_param_row(info, curcol))
		return TDS_CONVERT_FAIL;
	tdsdump_log(TDS_DBG_INFO2, "%s:%d\n", __FILE__, __LINE__);

	/* TODO what happen to data ?? */
	/* convert parameters */
	src_type = odbc_get_server_type(param->param_bindtype);
	if (src_type == TDS_FAIL)
		return TDS_CONVERT_FAIL;
	tdsdump_log(TDS_DBG_INFO2, "%s:%d\n", __FILE__, __LINE__);

	res = tds_convert(dbc->henv->tds_ctx, src_type, param->varaddr, *param->param_lenbind, dest_type, &ores);
	if (res < 0)
		return res;
	tdsdump_log(TDS_DBG_INFO2, "%s:%d\n", __FILE__, __LINE__);

	/* truncate ?? */
	if (res > curcol->column_size)
		res = curcol->column_size;

	/* free allocated memory */
	dest = &info->current_row[curcol->column_offset];
	switch (dest_type) {
	case SYBCHAR:
	case SYBVARCHAR:
	case XSYBCHAR:
	case XSYBVARCHAR:
		memcpy(&info->current_row[curcol->column_offset], ores.c, res);
		free(ores.c);
		break;
	case SYBTEXT:
		blob_info = (TDSBLOBINFO *) dest;
		if (blob_info->textvalue)
			free(blob_info->textvalue);
		blob_info->textvalue = ores.c;
		break;
	case SYBBINARY:
	case SYBVARBINARY:
	case XSYBBINARY:
	case XSYBVARBINARY:
		memcpy(&info->current_row[curcol->column_offset], ores.ib, res);
		free(ores.ib);
		break;
	case SYBIMAGE:
		blob_info = (TDSBLOBINFO *) dest;
		if (blob_info->textvalue)
			free(blob_info->textvalue);
		blob_info->textvalue = ores.ib;
		break;
	default:
		memcpy(&info->current_row[curcol->column_offset], &ores, res);
		break;
	}

	return res;
}
