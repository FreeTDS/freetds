/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2003, 2004 Frediano Ziglio
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

#include <assert.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: data.c,v 1.8 2004-09-23 11:33:38 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int tds_get_cardinal_type(int datatype);
static int tds_get_varint_size(int datatype);

/**
 * Set type of column initializing all dependency 
 * @param curcol column to set
 * @param type   type to set
 */
void
tds_set_column_type(TDSCOLUMN * curcol, int type)
{
	/* set type */
	curcol->on_server.column_type = type;
	curcol->column_type = tds_get_cardinal_type(type);

	/* set size */
	curcol->column_varint_size = tds_get_varint_size(type);
	if (curcol->column_varint_size == 0)
		curcol->column_cur_size = curcol->column_size = tds_get_size_by_type(type);

}

/**
 * Set type of column initializing all dependency 
 * @param curcol column to set
 * @param type   type to set
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
	}
	tds_set_column_type(curcol, type);

	if (is_collate_type(type)) {
		curcol->char_conv = tds->char_convs[is_unicode_type(type) ? client2ucs2 : client2server_chardata];
		memcpy(curcol->column_collation, tds->collation, sizeof(tds->collation));
	}

	/* special case, GUID, varint != 0 but only a size */
	/* TODO VARIANT, when supported */
	switch (type) {
	case SYBUNIQUE:
		curcol->column_size = sizeof(TDS_UNIQUE);
		break;
	default:
		break;
	}
}

static int
tds_get_cardinal_type(int datatype)
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
	}
	return datatype;
}

/**
 * tds_get_varint_size() returns the size of a variable length integer
 * returned in a TDS 7.0 result string
 */
static int
tds_get_varint_size(int datatype)
{
	switch (datatype) {
	case SYBLONGBINARY:
	case SYBTEXT:
	case SYBNTEXT:
	case SYBIMAGE:
		/* TODO support this strange type */
	case SYBVARIANT:
		return 4;
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
	case XSYBNCHAR:
	case XSYBNVARCHAR:
	case XSYBCHAR:
	case XSYBVARCHAR:
	case XSYBBINARY:
	case XSYBVARBINARY:
		return 2;
	default:
		return 1;
	}
}
