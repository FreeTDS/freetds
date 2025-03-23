/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 * Copyright (C) 2017  Frediano Ziglio
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

#define TDS_DONT_DEFINE_DEFAULT_FUNCTIONS
#include "common.h"
#include <assert.h>
#include <freetds/convert.h>
#include <freetds/checks.h>

static bool
is_convert_pointer_type(int type)
{
	switch (type) {
	case SYBCHAR: case SYBVARCHAR: case SYBTEXT: case XSYBCHAR: case XSYBVARCHAR:
	case SYBBINARY: case SYBVARBINARY: case SYBIMAGE: case XSYBBINARY: case XSYBVARBINARY:
	case SYBLONGBINARY:
		return true;
	}
	return false;
}


static void
free_convert(int type, CONV_RESULT *cr)
{
	if (is_convert_pointer_type(type))
		free(cr->c);
}

static void create_type(TDSSOCKET *tds, int desttype, int server_type, tds_any_type_t *func);

enum { TDSVER_MS = 0x704, TDSVER_SYB = 0x500 };

void tds_all_types(TDSSOCKET *tds, tds_any_type_t *func)
{
	int desttype;

	tds->conn->tds_version = TDSVER_MS;

	/*
	 * Test every type
	 */
	for (desttype = 0; desttype < 0x100; desttype++) {
		int types_buffer[10];
		int *types = types_buffer, *types_end;
		int server_type;
		int varint, other_varint;

		if (!is_tds_type_valid(desttype))
			continue;

		tds->conn->tds_version = TDSVER_MS;

		/* if is only Sybase change version to Sybase */
		varint = tds_get_varint_size(tds->conn, desttype);
		tds->conn->tds_version ^= TDSVER_MS ^ TDSVER_SYB;
		other_varint = tds_get_varint_size(tds->conn, desttype);
		tds->conn->tds_version ^= TDSVER_MS ^ TDSVER_SYB;
		if (varint == 1 && varint != other_varint)
			tds->conn->tds_version = TDSVER_SYB;

		server_type = desttype;
		switch (desttype) {
		/* unsupported */
		case SYBVOID:
		case SYBINTERVAL:
			continue;
		/* nullable, use another type */
		/* TODO, try all sizes */
		case SYBINTN:
			*types++ = SYBINT1;
			*types++ = SYBINT2;
			*types++ = SYBINT4;
			*types++ = SYBINT8;
			break;
		case SYBUINTN:
			*types++ = SYBUINT1;
			*types++ = SYBUINT2;
			*types++ = SYBUINT4;
			*types++ = SYBUINT8;
			tds->conn->tds_version = TDSVER_SYB;
			break;
		case SYBFLTN:
			*types++ = SYBREAL;
			*types++ = SYBFLT8;
			break;
		case SYBMONEYN:
			*types++ = SYBMONEY4;
			*types++ = SYBMONEY;
			break;
		case SYBDATETIMN:
			*types++ = SYBDATETIME4;
			*types++ = SYBDATETIME;
			break;
		case SYBDATEN:
			*types++ = SYBDATE;
			tds->conn->tds_version = TDSVER_SYB;
			break;
		case SYBTIMEN:
			*types++ = SYBTIME;
			tds->conn->tds_version = TDSVER_SYB;
			break;
		case SYB5INT8:
			*types++ = SYBINT8;
			break;
		/* TODO tds_set_column_type */

		case SYBXML: /* ?? */
		case SYBNTEXT: /* ?? */
		case XSYBNVARCHAR:
		case XSYBNCHAR:
		case SYBNVARCHAR:
		case SYBMSUDT:
		case SYBMSXML:
		case SYBUNITEXT:
		case SYBVARIANT: /* TODO */
		case SYBSINT1: /* TODO */
		case SYBMSTABLE: /* TODO */
			continue;
		}

		if (types == types_buffer)
			*types++ = desttype;

		types_end = types;
		for (types = types_buffer; types != types_end; ++types) {
			create_type(tds, *types, server_type, func);
			if (*types != server_type)
				create_type(tds, *types, *types, func);
		}
	}
}

static void create_type(TDSSOCKET *tds, int desttype, int server_type, tds_any_type_t *func)
{
	const TDSCONTEXT *ctx = tds_get_ctx(tds);
	int result;
	TDS_CHAR *src = NULL;
	TDS_UINT srclen;

	CONV_RESULT cr;
	const int srctype = SYBCHAR;

	TDSRESULTINFO *results;
	TDSCOLUMN *curcol;

	cr.n.precision = 8;
	cr.n.scale = 2;

	switch (desttype) {
	case SYBCHAR:
	case SYBVARCHAR:
	case SYBTEXT:
	case XSYBVARCHAR:
	case XSYBCHAR:
		src = "test of a character field";
		break;
	case SYBDATETIME:
	case SYBDATETIME4:
		src = "Apr 12, 1985 17:49:41";
		break;
	case SYBMSDATE:
	case SYBDATE:
		src = "2012-11-27";
		break;
	case SYBTIME:
		src = "15:27:12";
		break;
	case SYBMSTIME:
	case SYB5BIGTIME:
		src = "15:27:12.327862";
		break;
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
	case SYB5BIGDATETIME:
		src = "2015-09-12 21:48:12.638161";
		break;
	case SYBBINARY:
	case SYBIMAGE:
		src = "0xbeef";
		break;
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBUINT1:
	case SYBUINT2:
	case SYBUINT4:
		src = "255";
		break;
	case SYBINT8:
	case SYBUINT8:
		src = "374632567765";
		break;
	case SYBFLT8:
	case SYBREAL:
	case SYBMONEY:
	case SYBMONEY4:
		src = "1237.45";
		cr.n.precision = 8;
		cr.n.scale = 2;
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		src = "947919.25";
		cr.n.precision = 8;
		cr.n.scale = 2;
		break;
	case SYBUNIQUE:
		src = "A8C60F70-5BD4-3E02-B769-7CCCCA585DCC";
		break;
	case SYBBIT:
	default:
		src = "1";
		break;
	}
	assert(src);
	srclen = (TDS_UINT) strlen(src);

	/*
	 * Now at last do the conversion
	 */

	result = tds_convert(ctx, srctype, src, srclen, desttype, &cr);

	if (result < 0) {
		if (result == TDS_CONVERT_NOAVAIL)	/* tds_willconvert returned true, but it lied. */
			fprintf(stderr, "Conversion not yet implemented:\n\t");

		fprintf(stderr, "failed (%d) to convert %d (%s, %d bytes) : %d (%s).\n",
			result,
			srctype, tds_prtype(srctype), srclen,
			desttype, tds_prtype(desttype));

		if (result == TDS_CONVERT_NOAVAIL)
			exit(1);
	}

	printf("converted %d (%s, %d bytes) -> %d (%s, %d bytes).\n",
	       srctype, tds_prtype(srctype), srclen,
	       desttype, tds_prtype(desttype), result);

	results = tds_alloc_param_result(NULL);
	assert(results);
	curcol = results->columns[0];
	assert(curcol);

	tds_set_column_type(tds->conn, curcol, server_type);
	curcol->on_server.column_size = curcol->column_size = curcol->column_cur_size = result;
	if (is_numeric_type(desttype)) {
		curcol->column_prec = cr.n.precision;
		curcol->column_scale = cr.n.scale;
	}
	switch (desttype) {
	case SYB5BIGDATETIME:
	case SYB5BIGTIME:
		curcol->column_prec = curcol->column_scale = 6;
		break;
	}
	CHECK_COLUMN_EXTRA(curcol);

	tds_alloc_param_data(curcol);
	if (is_blob_col(curcol)) {
		((TDSBLOB *) curcol->column_data)->textvalue = cr.c;
		cr.c = NULL;
	} else if (is_convert_pointer_type(desttype)) {
		memcpy(curcol->column_data, cr.c, (size_t) result);
	} else {
		memcpy(curcol->column_data, &cr.i, (size_t) result);
	}

	func(tds, curcol);

	/* try to convert to SQL_VARIANT */
	if (is_variant_inner_type(desttype)) {
		TDSVARIANT *v;
		TDSCOLUMN *basecol;

		tds->conn->tds_version = TDSVER_MS;

		results = tds_alloc_param_result(results);
		assert(results);

		basecol = results->columns[0];
		curcol = results->columns[1];
		tds_set_column_type(tds->conn, curcol, SYBVARIANT);
		CHECK_COLUMN_EXTRA(curcol);

		tds_alloc_param_data(curcol);
		v = (TDSVARIANT *) curcol->column_data;
		v->size = basecol->column_size;
		v->type = basecol->column_type;
		memcpy(v->collation, basecol->column_collation, sizeof(v->collation));

		assert(basecol->column_cur_size > 0);
		v->data = tds_new(TDS_CHAR, (size_t) basecol->column_cur_size);
		assert(v->data);
		v->data_len = basecol->column_cur_size;
		curcol->column_cur_size = v->data_len;
		curcol->column_size = curcol->on_server.column_size = 8009;
		assert(!is_blob_col(basecol));
		assert(v->data_len >= 0);
		memcpy(v->data, basecol->column_data, (size_t) v->data_len);
		CHECK_COLUMN_EXTRA(curcol);

		func(tds, curcol);
	}

	tds_free_results(results);

	if (result >= 0)
		free_convert(desttype, &cr);
}
