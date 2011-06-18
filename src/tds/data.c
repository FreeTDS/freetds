/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2003-2011 Frediano Ziglio
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

#include "tds.h"
#include "tds_checks.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: data.c,v 1.33 2011-06-18 17:52:24 freddy77 Exp $");

/**
 * Set type of column initializing all dependency 
 * @param curcol column to set
 * @param type   type to set
 */
void
tds_set_column_type(TDSSOCKET * tds, TDSCOLUMN * curcol, int type)
{
	/* set type */
	curcol->on_server.column_type = type;
	curcol->column_type = tds_get_cardinal_type(type, curcol->column_usertype);

	/* set size */
	curcol->column_cur_size = -1;
	curcol->column_varint_size = tds_get_varint_size(tds, type);
	if (curcol->column_varint_size == 0)
		curcol->column_cur_size = curcol->on_server.column_size = curcol->column_size = tds_get_size_by_type(type);

}

/**
 * Set type of column initializing all dependency
 * \param tds    state information for the socket and the TDS protocol
 * \param curcol column to set
 * \param type   type to set
 */
void
tds_set_param_type(TDSSOCKET * tds, TDSCOLUMN * curcol, TDS_SERVER_TYPE type)
{
	if (IS_TDS7_PLUS(tds)) {
		switch (type) {
		case SYBVARCHAR:
			type = XSYBVARCHAR;
			break;
		case SYBCHAR:
			type = XSYBCHAR;
			break;
		case SYBVARBINARY:
			type = XSYBVARBINARY;
			break;
		case SYBBINARY:
			type = XSYBBINARY;
			break;
			/* avoid warning on other types */
		default:
			break;
		}
	} else if (IS_TDS50(tds)) {
		if (type == SYBINT8)
			type = SYB5INT8;
	}
	tds_set_column_type(tds, curcol, type);

	if (is_collate_type(type)) {
		curcol->char_conv = tds->char_convs[is_unicode_type(type) ? client2ucs2 : client2server_chardata];
		memcpy(curcol->column_collation, tds->collation, sizeof(tds->collation));
	}

	/* special case, GUID, varint != 0 but only a size */
	/* TODO VARIANT, when supported */
	switch (type) {
	case SYBUNIQUE:
		curcol->on_server.column_size = curcol->column_size = sizeof(TDS_UNIQUE);
		break;
	case SYBBITN:
		curcol->on_server.column_size = curcol->column_size = sizeof(TDS_TINYINT);
		break;
	case SYBMSDATE:
		curcol->on_server.column_size = curcol->column_size = 4;
		break;
	/* mssql 2005 don't like SYBINT4 as parameter closing connection  */
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBINT8:
		curcol->on_server.column_type = SYBINTN;
		curcol->column_varint_size = 1;
		curcol->column_cur_size = -1;
		break;
	case SYBMONEY4:
	case SYBMONEY:
		curcol->on_server.column_type = SYBMONEYN;
		curcol->column_varint_size = 1;
		curcol->column_cur_size = -1;
		break;
	case SYBDATETIME:
	case SYBDATETIME4:
		curcol->on_server.column_type = SYBDATETIMN;
		curcol->column_varint_size = 1;
		curcol->column_cur_size = -1;
		break;
	case SYBFLT8:
	case SYBREAL:
		curcol->on_server.column_type = SYBFLTN;
		curcol->column_varint_size = 1;
		curcol->column_cur_size = -1;
		break;
	case SYBNTEXT:
		if (IS_TDS72_PLUS(tds)) {
			curcol->column_varint_size = 8;
			curcol->on_server.column_type = XSYBNVARCHAR;
		}
		break;
	case SYBTEXT:
		if (IS_TDS72_PLUS(tds)) {
			curcol->column_varint_size = 8;
			curcol->on_server.column_type = XSYBVARCHAR;
		}
		break;
	case SYBIMAGE:
		if (IS_TDS72_PLUS(tds)) {
			curcol->column_varint_size = 8;
			curcol->on_server.column_type = XSYBVARBINARY;
		}
		break;
	default:
		break;
	}
}

int
tds_get_cardinal_type(int datatype, int usertype)
{
	switch (datatype) {
	case XSYBVARBINARY:
		return SYBVARBINARY;
	case XSYBBINARY:
		return SYBBINARY;
	case SYBNTEXT:
		return SYBTEXT;
	case XSYBNVARCHAR:
	case XSYBVARCHAR:
		return SYBVARCHAR;
	case XSYBNCHAR:
	case XSYBCHAR:
		return SYBCHAR;
	case SYB5INT8:
		return SYBINT8;
	case SYBLONGBINARY:
		switch (usertype) {
		case USER_UNICHAR_TYPE:
		case USER_UNIVARCHAR_TYPE:
			return SYBTEXT;
		}
		break;
	}
	return datatype;
}

TDSRET
tds_data_get_info(TDSSOCKET *tds, TDSCOLUMN *col)
{
	switch (col->column_varint_size) {
	case 8:
		col->column_size = 0x7ffffffflu;
		break;
	case 5:
	case 4:
		col->column_size = tds_get_int(tds);
		break;
	case 2:
		/* assure > 0 */
		col->column_size = tds_get_smallint(tds);
		/* under TDS9 this means ?var???(MAX) */
		if (col->column_size < 0 && IS_TDS72_PLUS(tds)) {
			col->column_size = 0x3ffffffflu;
			col->column_varint_size = 8;
		}
		break;
	case 1:
		if (col->column_type == SYBMSDATE)
			col->column_size = 4;
		else
			col->column_size = tds_get_byte(tds);
		break;
	case 0:
		col->column_size = tds_get_size_by_type(col->column_type);
		break;
	}

	/* numeric and decimal have extra info */
	if (is_numeric_type(col->column_type)) {
		col->column_prec = tds_get_byte(tds);        /* precision */
		col->column_scale = tds_get_byte(tds);       /* scale */
		/* FIXME check prec/scale, don't let server crash us */
	}

	if (IS_TDS71_PLUS(tds) && is_collate_type(col->on_server.column_type)) {
		/* based on true type as sent by server */
		/*
		 * first 2 bytes are windows code (such as 0x409 for english)
		 * other 2 bytes ???
		 * last bytes is id in syscharsets
		 */
		tds_get_n(tds, col->column_collation, 5);
		col->char_conv =
			tds_iconv_from_collate(tds, col->column_collation);
	}

	/* Only read table_name for blob columns (eg. not for SYBLONGBINARY) */
	if (is_blob_type(col->on_server.column_type)) {
		/* discard this additional byte */
		if (IS_TDS72_PLUS(tds)) {
			unsigned char num_parts = tds_get_byte(tds);
			/* TODO do not discard first ones */
			for (; num_parts; --num_parts) {
				col->table_namelen =
				tds_get_string(tds, tds_get_smallint(tds), col->table_name, sizeof(col->table_name) - 1);
			}
		} else {
			col->table_namelen =
				tds_get_string(tds, tds_get_smallint(tds), col->table_name, sizeof(col->table_name) - 1);
		}
	} else if (IS_TDS72_PLUS(tds) && col->on_server.column_type == SYBMSXML) {
		unsigned char has_schema = tds_get_byte(tds);
		if (has_schema) {
			/* discard schema informations */
			tds_get_string(tds, tds_get_byte(tds), NULL, 0);        /* dbname */
			tds_get_string(tds, tds_get_byte(tds), NULL, 0);        /* schema owner */
			tds_get_string(tds, tds_get_smallint(tds), NULL, 0);    /* schema collection */
		}
	}
	return TDS_SUCCESS;
}

#include "tds_types.h"
