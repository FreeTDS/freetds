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

/**
 * \page new_type How to add a new type
 * \section intro Introduction
 * Adding a new type in FreeTDS is a quite complicated task involving
 * different tasks.
 *
 * To see an example you can look at
 * \commit{adb893f1381fd3ea40564c775e30dc8cdc81dcf2}
 * ("Implement big(date)time types") and parent changes in the source
 * repository.
 *
 * \section tds libTDS changes
 * <ul>
 * <li>protocol. First thing to do is add the type to the protocol.
 *    A type usually have some mnemonic constant and a structure.
 *    Declare them in \c include/freetds/proto.h file. Note that
 *    here you should declare the structure the server use not
 *    the structure to hold the data in libTDS.
 *    <br>Cfr \commit{a74a06e1f97f3137f6cf1bc7319dd7a2cfb52b1f}.
 *
 * <li>base information. Add the type to \c misc/types.csv file
 *    (I use LibreOffice Calc to do it). This table maintain the
 *    base information for a type.
 *    <br>Cfr \commit{680cb3371e042bb372cbc5e6feb4054e50d40c1a}.
 *
 * <li>data. There should be some code to handle this type to/from
 *    the server. This code is implemented in \c include/freetds/data.h
 *    and \c src/tds/data.c. You can either add a new set of functions
 *    to handle this new type or add the type handling do another set
 *    of types depending on how complicated is that type.
 *    One thing you have to to at this step is determine how you store
 *    that type in libTDS. This is quite important at upper level
 *    libraries will have to use these structures or even present
 *    these data to client code (like DB-Library usually do).
 *    Due to the way FreeTDS works now you would get a linker error
 *    in the ODBC part. You can either ignore the error and proceed
 *    with libTDS, add the code to ODBC or disable temporarily ODBC.
 *    <br>Cfr \commit{680cb3371e042bb372cbc5e6feb4054e50d40c1a}.
 *
 * <li>enable the type from server. In order to receive the new type
 *    from the server you have to tell the server that we support
 *    that type. This can be either done changing the protocol (usually
 *    Microsoft) or enabling some flags (capabilities for Sybase).
 *    <br>Cfr \commit{a498703ff9e309c656b19dd990f4cad0283a47c7}.
 *
 * <li>conversions. Conversions are not hard to write but usually
 *    require quite a bit of coding. After extending CONV_RESULT
 *    type in \c include/freetds/convert.h and adding the type to
 *    the script that generate the conversion tables in
 *    \c src/tds/tds_willconvert.pl you have to write the big part
 *    in \c src/tds/covnert.c. You have to implement all kind of
 *    conversions you declared in the previous file. Reuse the
 *    functions that are there (for instance there are some
 *    parser functions). Also if there are similar types it could
 *    be helpful to convert first your type to a super type then
 *    use the conversion for that type. For instance for SMALLINT
 *    type (\c tds_convert_int2) the type is just readed and then
 *    \c tds_convert_int is called which handle any int (actually
 *    32 bit integer). Same for data where the \c TDS_DATETIMEALL
 *    structure is used. Note that conversions to binary (which
 *    usually are implemented) are done in another function
 *    (\c tds_convert_to_binary).
 *    <br>Cfr \commit{9ed52cb78f725607ac109c8c284ca7c4658d87a9}.
 *
 * <li>string definition. Add string for your type to
 *    \c src/tds/token.c in \c tds_prtype.
 *    <br>Cfr \commit{ac0d3b46db7d98436cd76f906b7d455f7651faae}.
 *
 * <li>conversion tests. You probably will have done some mistake
 *    with conversions but don't mind, there are some tests which
 *    will help sorting this out.
 *    \c src/tds/unittests/convert.c
 *    try any possible combination of conversion to check if
 *    all conversion are implemented (it does not check the
 *    conversions themself).
 *    \c src/tds/unittests/t0007.c test that your conversion
 *    are working. Just add manually the conversions you want
 *    to try.
 *    <br>Cfr \commit{abcc09c9a88acd0e9a45b46dab3ca44309917a02}.
 *
 * <li>parameter. Add type/parameter declaration in
 *    \c tds_get_column_declaration in \c src/tds/query.c.
 *    Also do any necessary step to initialize the parameter
 *    to send to server.
 *    <br>Cfr \commit{54fdd3233e430c045cf5524ac385770738d9e92c},
 *    \commit{88cfea19d91245372779b8893a2d62b42696cd49}.
 *
 * <li>emulated prepared/rpc. If needed handle your type
 *    in \c tds_put_param_as_string in \c src/tds/query.c.
 *    <br>Cfr \commit{017b7bf2fee0f09847e64546d27382d2f2b756f4}.
 *
 * </ul>
 *
 * \section odbc ODBC changes
 * ODBC is the most complicated library to add a type to.
 * Usually its data are different from libTDS so you have to add additional
 * code for conversions which are not required by other libraries.
 * <ul>
 * <li>type information. Every type in ODBC have related information.
 *    These information are set in \c src/odbc/odbc_data.c.
 *    Depending on the changes you did for data in libTDS you should
 *    handle the new type.
 *    <br>Cfr \commit{71e189e206dc9b6f6513e0aa0e4133a4f8dec110}.
 *
 * <li>type information test. Related to the previous change there
 *    is \c src/odbc/unittests/describecol.c test. Add a test case
 *    for new type. You should attempt to run same test also on
 *    proprietary library if possible.
 *    <br>Cfr \commit{8a8ec16a6a514a5d6ac66c2470eff51f6a8d4a53}.
 *
 * <li>conversions from odbc. Define how the ODBC type should convert
 *    to the server and implement the conversion.
 *    <br>Cfr \commit{29606cbf413c44e49ddfcfb8a93b8a6bd2565a84},
 *    \commit{87c84e20a594472a72990b12d4a1451b22e6714b}.
 *
 * <li>conversions to binary. Binary representation in ODBC are usually
 *    different from server ones. If so implement the proper conversions.
 *    <br>Cfr \commit{56009f35d3e0def339a0c5cb98d006e5e710d523}.
 *
 * <li>conversions to characters. Same problem for character types.
 *    <br>Cfr \commit{25ff091880dabc32f28a73f09bf31c01314aca2f}.
 *
 * <li>conversion test. You probably want to test ODBC conversions.
 *    This can be done changing \c src/odbc/unittests/data.c test and
 *    \c src/odbc/unittests/genparams.c.
 *    <br>Cfr \commit{e69f7d564dac44884f7c5f0106cceafce4af168b}.
 * </ul>
 *
 * \section ctlib CT-Library changes
 * This is quite easy as usual the conversion in libTDS are fine for
 * this library.
 * <ul>
 * <li>define type in \c include/cspublic.h
 * <li>implement conversion in \c src/ctlib/cs.h
 * <li>set corrent conversion from cs types to server in
 *    \c src/ctlib/ct.c
 * </ul>
 * Cfr \commit{c5e71e5ad4a557038ecedcec457e2531ab02a77b}.
 *
 * \section dblib DB-Library changes
 * A bit more complicated than CT-Library but not that much.
 * <ul>
 * <li>add type and binding type to \c include/sybdb.h
 * <li>add NULL handling in \c dbgetnull, \c dbsetnull
 *    and \c default_null_representation in
 *    \c src/dblib/dblib.c
 * <li>add binding to dbbindtype
 * <li>add support for conversion from/to server
 * <li>add printable size
 * <li>return correct type string
 * </ul>
 * Cfr \commit{99dd126e0eb248dd3079b2a7cf97437fe3bcd163}.
 *
 * \section apps Applications changes
 * datacopy application requires some changes too to support new types
 * so add them to \c src/apps/datacopy.c.
 * <br>Cfr \commit{e59c48ac39c76abb036651f8ec238090eef321c9}.
 */

/**
 * @file
 * @brief Handle different data handling from network
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

#define TDS_DONT_DEFINE_DEFAULT_FUNCTIONS
#include <freetds/utils.h>
#include <freetds/tds.h>
#include <freetds/bytes.h>
#include <freetds/iconv.h>
#include <freetds/checks.h>
#include <freetds/stream.h>
#include <freetds/data.h>

#define USE_ICONV (tds->conn->use_iconv)

static const TDSCOLUMNFUNCS *tds_get_column_funcs(TDSCONNECTION *conn, int type);
#ifdef WORDS_BIGENDIAN
static void tds_swap_datatype(int coltype, void *b);
#endif
static void tds_swap_numeric(TDS_NUMERIC *num);

#undef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#undef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

/**
 * Set type of column initializing all dependency.
 * column_usertype should already be set.
 * @param curcol column to set
 * @param type   type to set
 */
void
tds_set_column_type(TDSCONNECTION * conn, TDSCOLUMN * curcol, TDS_SERVER_TYPE type)
{
	/* set type */
	curcol->on_server.column_type = type;
	curcol->funcs = tds_get_column_funcs(conn, type);
	curcol->column_type = tds_get_cardinal_type(type, curcol->column_usertype);

	/* set size */
	curcol->column_cur_size = -1;
	curcol->column_varint_size = tds_get_varint_size(conn, type);
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
tds_set_param_type(TDSCONNECTION * conn, TDSCOLUMN * curcol, TDS_SERVER_TYPE type)
{
	if (IS_TDS7_PLUS(conn)) {
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
		case SYBBIT:
			type = SYBBITN;
			break;
			/* avoid warning on other types */
		default:
			break;
		}
	} else if (IS_TDS50(conn)) {
		switch (type) {
		case SYBINT8:
			type = SYB5INT8;
			break;
			/* avoid warning on other types */
		default:
			break;
		}
	}
	tds_set_column_type(conn, curcol, type);

	if (is_collate_type(type) || is_char_type(type)) {
		curcol->char_conv = conn->char_convs[is_unicode_type(type) ? client2ucs2 : client2server_chardata];
		memcpy(curcol->column_collation, conn->collation, sizeof(conn->collation));
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
		if (IS_TDS72_PLUS(conn)) {
			curcol->column_varint_size = 8;
			curcol->on_server.column_type = XSYBNVARCHAR;
		}
		break;
	case SYBTEXT:
		if (IS_TDS72_PLUS(conn)) {
			curcol->column_varint_size = 8;
			curcol->on_server.column_type = XSYBVARCHAR;
		}
		break;
	case SYBIMAGE:
		if (IS_TDS72_PLUS(conn)) {
			curcol->column_varint_size = 8;
			curcol->on_server.column_type = XSYBVARBINARY;
		}
		break;
	case SYB5BIGTIME:
	case SYB5BIGDATETIME:
		curcol->column_prec = 6;
		curcol->column_scale = 6;
		break;
	default:
		break;
	}
}

TDS_SERVER_TYPE
tds_get_cardinal_type(TDS_SERVER_TYPE datatype, int usertype)
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
	case SYBMSXML:
		return SYBLONGCHAR;
	default:
		break;
	}
	return datatype;
}

TDSRET
tds_generic_get_info(TDSSOCKET *tds, TDSCOLUMN *col)
{
	switch (col->column_varint_size) {
	case 8:
		col->column_size = 0x7ffffffflu;
		break;
	case 5:
	case 4:
		col->column_size = tds_get_int(tds);
		if (col->column_size < 0)
			return TDS_FAIL;
		break;
	case 2:
		/* assure > 0 */
		col->column_size = tds_get_smallint(tds);
		/* under TDS7.2 this means ?var???(MAX) */
		if (col->column_size < 0 && IS_TDS72_PLUS(tds->conn)) {
			col->column_size = 0x3ffffffflu;
			col->column_varint_size = 8;
		}
		if (col->column_size < 0)
			return TDS_FAIL;
		break;
	case 1:
		col->column_size = tds_get_byte(tds);
		break;
	case 0:
		col->column_size = tds_get_size_by_type(col->column_type);
		break;
	}

	if (IS_TDS71_PLUS(tds->conn) && is_collate_type(col->on_server.column_type)) {
		/* based on true type as sent by server */
		/*
		 * first 2 bytes are windows code (such as 0x409 for english)
		 * other 2 bytes ???
		 * last bytes is id in syscharsets
		 */
		tds_get_n(tds, col->column_collation, 5);
		col->char_conv =
			tds_iconv_from_collate(tds->conn, col->column_collation);
	}

	/* Only read table_name for blob columns (eg. not for SYBLONGBINARY) */
	if (is_blob_type(col->on_server.column_type)) {
		/* discard this additional byte */
		if (IS_TDS72_PLUS(tds->conn)) {
			unsigned char num_parts = tds_get_byte(tds);
			/* TODO do not discard first ones */
			for (; num_parts; --num_parts) {
				tds_dstr_get(tds, &col->table_name, tds_get_usmallint(tds));
			}
		} else {
			tds_dstr_get(tds, &col->table_name, tds_get_usmallint(tds));
		}
	} else if (IS_TDS72_PLUS(tds->conn) && col->on_server.column_type == SYBMSXML) {
		unsigned char has_schema = tds_get_byte(tds);
		if (has_schema) {
			/* discard schema informations */
			tds_get_string(tds, tds_get_byte(tds), NULL, 0);        /* dbname */
			tds_get_string(tds, tds_get_byte(tds), NULL, 0);        /* schema owner */
			tds_get_string(tds, tds_get_usmallint(tds), NULL, 0);    /* schema collection */
		}
	}
	return TDS_SUCCESS;
}

/* tds_generic_row_len support also variant and return size to hold blob */
TDS_COMPILE_CHECK(variant_size, sizeof(TDSBLOB) >= sizeof(TDSVARIANT));

TDS_INT
tds_generic_row_len(TDSCOLUMN *col)
{
	CHECK_COLUMN_EXTRA(col);

	if (is_blob_col(col))
		return sizeof(TDSBLOB);
	return col->column_size;
}

static TDSRET
tds_get_char_dynamic(TDSSOCKET *tds, TDSCOLUMN *curcol, void **pp, size_t allocated, TDSINSTREAM *r_stream)
{
	TDSRET res;
	TDSDYNAMICSTREAM w;

	/*
	 * Blobs don't use a column's fixed buffer because the official maximum size is 2 GB.
	 * Instead, they're reallocated as necessary, based on the data's size.
	 */
	res = tds_dynamic_stream_init(&w, pp, allocated);
	if (TDS_FAILED(res))
		return res;

	if (USE_ICONV && curcol->char_conv)
		res = tds_convert_stream(tds, curcol->char_conv, to_client, r_stream, &w.stream);
	else
		res = tds_copy_stream(r_stream, &w.stream);
	curcol->column_cur_size = w.size;
	return res;
}

typedef struct tds_varmax_stream {
	TDSINSTREAM stream;
	TDSSOCKET *tds;
	TDS_INT chunk_left;
} TDSVARMAXSTREAM;

static int
tds_varmax_stream_read(TDSINSTREAM *stream, void *ptr, size_t len)
{
	TDSVARMAXSTREAM *s = (TDSVARMAXSTREAM *) stream;

	/* read chunk len if needed */
	if (s->chunk_left == 0) {
		TDS_INT l = tds_get_int(s->tds);
		if (l <= 0) l = -1;
		s->chunk_left = l;
	}

	/* no more data ?? */
	if (s->chunk_left < 0)
		return 0;

	/* read part of data */
	if (len > s->chunk_left)
		len = s->chunk_left;
	s->chunk_left -= len;
	if (tds_get_n(s->tds, ptr, len))
		return len;
	return -1;
}

static TDSRET
tds72_get_varmax(TDSSOCKET * tds, TDSCOLUMN * curcol)
{
	TDS_INT8 len;
	TDSVARMAXSTREAM r;
	size_t allocated = 0;
	void **pp = (void**) &(((TDSBLOB*) curcol->column_data)->textvalue);

	len = tds_get_int8(tds);

	/* NULL */
	if (len == -1) {
		curcol->column_cur_size = -1;
		return TDS_SUCCESS;
	}

	/* try to allocate an initial buffer */
	if (len > (TDS_INT8) (~((size_t) 0) >> 1))
		return TDS_FAIL;
	if (len > 0) {
		TDS_ZERO_FREE(*pp);
		allocated = (size_t) len;
		if (is_unicode_type(curcol->on_server.column_type))
			allocated /= 2;
	}

	r.stream.read = tds_varmax_stream_read;
	r.tds = tds;
	r.chunk_left = 0;

	return tds_get_char_dynamic(tds, curcol, pp, allocated, &r.stream);
}

TDS_COMPILE_CHECK(tds_variant_size,  sizeof(((TDSVARIANT*)0)->data) == sizeof(((TDSBLOB*)0)->textvalue));
TDS_COMPILE_CHECK(tds_variant_offset,TDS_OFFSET(TDSVARIANT, data) == TDS_OFFSET(TDSBLOB, textvalue));

/*
 * This strange type has following structure 
 * 0 len (int32) -- NULL 
 * len (int32), type (int8), data -- ints, date, etc
 * len (int32), type (int8), 7 (int8), collation, column size (int16) -- [n]char, [n]varchar, binary, varbinary 
 * BLOBS (text/image) not supported
 */
TDSRET
tds_variant_get(TDSSOCKET * tds, TDSCOLUMN * curcol)
{
	unsigned int colsize = tds_get_uint(tds);
	int varint;
	TDS_SERVER_TYPE type;
	TDS_UCHAR info_len;
	TDSVARIANT *v;
	TDSRET rc;

	/* NULL */
	curcol->column_cur_size = -1;
	if (colsize < 2) {
		tds_get_n(tds, NULL, colsize);
		return TDS_SUCCESS;
	}

	type = (TDS_SERVER_TYPE) tds_get_byte(tds);
	info_len = tds_get_byte(tds);
	if (!is_tds_type_valid(type))
		goto error_type;
	v = (TDSVARIANT*) curcol->column_data;
	v->type = type;
	colsize -= 2;
	if (info_len > colsize)
		goto error_type;
	if (is_collate_type(type)) {
		if (sizeof(v->collation) > info_len)
			goto error_type;
		tds_get_n(tds, v->collation, sizeof(v->collation));
		colsize -= sizeof(v->collation);
		info_len -= sizeof(v->collation);
		curcol->char_conv = is_unicode_type(type) ? 
			tds->conn->char_convs[client2ucs2] : tds_iconv_from_collate(tds->conn, v->collation);
	}

	/* special case for numeric */
	if (is_numeric_type(type)) {
		TDS_NUMERIC *num;
		if (info_len != 2)
			goto error_type;
		if (v->data)
			TDS_ZERO_FREE(v->data);
		v->data_len = sizeof(TDS_NUMERIC);
		num = tds_new0(TDS_NUMERIC, 1);
		v->data = (TDS_CHAR *) num;
		num->precision = tds_get_byte(tds);
		num->scale     = tds_get_byte(tds);
		colsize -= 2;
		/* check prec/scale, don't let server crash us */
		if (num->precision < 1 || num->precision > MAXPRECISION
		    || num->scale > num->precision)
			goto error_type;
		if (colsize > sizeof(num->array))
			goto error_type;
		curcol->column_cur_size = colsize;
		tds_get_n(tds, num->array, colsize);
		if (IS_TDS7_PLUS(tds->conn))
			tds_swap_numeric(num);
		return TDS_SUCCESS;
	}

	/* special case for MS date/time */
	switch (type) {
	case SYBMSTIME:
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
		if (info_len != 1)
			goto error_type;
		curcol->column_scale = curcol->column_prec = tds_get_byte(tds);
		if (curcol->column_prec > 7)
			goto error_type;
		colsize -= info_len;
		info_len = 0;
		/* fall through */
	case SYBMSDATE:
		if (info_len != 0)
			goto error_type;
		/* dirty trick */
		tds->in_buf[--tds->in_pos] = colsize;
		if (v->data)
			TDS_ZERO_FREE(v->data);
		v->data_len = sizeof(TDS_DATETIMEALL);
		v->data = tds_new0(TDS_CHAR, sizeof(TDS_DATETIMEALL));
		curcol->column_type = type;
		curcol->column_data = (unsigned char *) v->data;
		/* trick, call get function */
		rc = tds_msdatetime_get(tds, curcol);
		curcol->column_type = SYBVARIANT;
		curcol->column_data = (unsigned char *) v;
		return rc;
	default:
		break;
	}
	varint = (type == SYBUNIQUE) ? 0 : tds_get_varint_size(tds->conn, type);
	if (varint != info_len || varint > 2)
		goto error_type;
	switch (varint) {
	case 0:
		v->size = tds_get_size_by_type(type);
		break;
	case 1:
		v->size = tds_get_byte(tds);
		break;
	case 2:
		v->size = tds_get_smallint(tds);
		break;
	default:
		goto error_type;
	}
	colsize -= info_len;
	curcol->column_cur_size = colsize;
	if (v->data)
		TDS_ZERO_FREE(v->data);
	if (colsize) {
		TDSRET res;
		TDSDATAINSTREAM r;

		if (USE_ICONV && curcol->char_conv)
			v->type = tds_get_cardinal_type(type, 0);

		tds_datain_stream_init(&r, tds, colsize);
		res = tds_get_char_dynamic(tds, curcol, (void **) &v->data, colsize, &r.stream);
		if (TDS_FAILED(res))
			return res;
		colsize = curcol->column_cur_size;
#ifdef WORDS_BIGENDIAN
		if (tds->conn->emul_little_endian)
			tds_swap_datatype(tds_get_conversion_type(type, colsize), v->data);
#endif
	}
	v->data_len = colsize;
	CHECK_COLUMN_EXTRA(curcol);
	return TDS_SUCCESS;

error_type:
	tds_get_n(tds, NULL, colsize);
	return TDS_FAIL;
}

/**
 * Read a data from wire
 * \param tds state information for the socket and the TDS protocol
 * \param curcol column where store column information
 * \return TDS_FAIL on error or TDS_SUCCESS
 */
TDSRET
tds_generic_get(TDSSOCKET * tds, TDSCOLUMN * curcol)
{
	unsigned char *dest;
	int len, colsize;
	int fillchar;
	TDSBLOB *blob = NULL;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(curcol);

	tdsdump_log(TDS_DBG_INFO1, "tds_get_data: type %d, varint size %d\n", curcol->column_type, curcol->column_varint_size);
	switch (curcol->column_varint_size) {
	case 4:
		/* It's a BLOB... */
		len = tds_get_byte(tds);
		blob = (TDSBLOB *) curcol->column_data;
		if (len == 16) {	/*  Jeff's hack */
			tds_get_n(tds, blob->textptr, 16);
			tds_get_n(tds, blob->timestamp, 8);
			blob->valid_ptr = 1;
			if (IS_TDS72_PLUS(tds->conn) &&
			    memcmp(blob->textptr, "dummy textptr\0\0",16) == 0)
				blob->valid_ptr = 0;
			colsize = tds_get_int(tds);
		} else {
			colsize = -1;
		}
		break;
	case 5:
		colsize = tds_get_int(tds);
		if (colsize == 0)
			colsize = -1;
		break;
	case 8:
		return tds72_get_varmax(tds, curcol);
	case 2:
		colsize = tds_get_smallint(tds);
		break;
	case 1:
		colsize = tds_get_byte(tds);
		if (colsize == 0)
			colsize = -1;
		break;
	case 0:
		/* TODO this should be column_size */
		colsize = tds_get_size_by_type(curcol->column_type);
		break;
	default:
		colsize = -1;
		break;
	}
	if (IS_TDSDEAD(tds))
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_get_data(): wire column size is %d \n", colsize);
	/* set NULL flag in the row buffer */
	if (colsize < 0) {
		curcol->column_cur_size = -1;
		return TDS_SUCCESS;
	}

	/* 
	 * We're now set to read the data from the wire.  For varying types (e.g. char/varchar)
	 * make sure that curcol->column_cur_size reflects the size of the read data, 
	 * after any charset conversion.  tds_get_char_data() does that for you, 
	 * but of course tds_get_n() doesn't.  
	 *
	 * colsize == wire_size, bytes to read
	 * curcol->column_cur_size == sizeof destination buffer, room to write
	 */
	dest = curcol->column_data;
	if (is_blob_col(curcol)) {
		TDSDATAINSTREAM r;
		size_t allocated;
		TDSRET ret;

		blob = (TDSBLOB *) dest; 	/* cf. column_varint_size case 4, above */

		/* empty string */
		if (colsize == 0) {
			curcol->column_cur_size = 0;
			if (blob->textvalue)
				TDS_ZERO_FREE(blob->textvalue);
			return TDS_SUCCESS;
		}

		allocated = MAX(curcol->column_cur_size, 0);
		if (colsize > allocated) {
			TDS_ZERO_FREE(blob->textvalue);
			allocated = colsize;
			if (is_unicode_type(curcol->on_server.column_type))
				allocated /= 2;
		}

		tds_datain_stream_init(&r, tds, colsize);
		ret = tds_get_char_dynamic(tds, curcol, (void **) &blob->textvalue, allocated, &r.stream);
		if (TDS_FAILED(ret) && TDS_UNLIKELY(r.wire_size > 0)) {
			tds_get_n(tds, NULL, r.wire_size);
			return ret;
		}
		return TDS_SUCCESS;
	}

	/* non-numeric and non-blob */

	if (USE_ICONV && curcol->char_conv) {
		if (TDS_FAILED(tds_get_char_data(tds, (char *) dest, colsize, curcol)))
			return TDS_FAIL;
	} else {
		/*
		 * special case, some servers seem to return more data in some conditions
		 * (ASA 7 returning 4 byte nullable integer)
		 */
		int discard_len = 0;
		if (colsize > curcol->column_size) {
			discard_len = colsize - curcol->column_size;
			colsize = curcol->column_size;
		}
		if (!tds_get_n(tds, dest, colsize))
			return TDS_FAIL;
		if (discard_len > 0)
			tds_get_n(tds, NULL, discard_len);
		curcol->column_cur_size = colsize;
	}

	/* pad (UNI)CHAR and BINARY types */
	fillchar = 0;
	switch (curcol->column_type) {
	/* extra handling for SYBLONGBINARY */
	case SYBLONGBINARY:
		if (curcol->column_usertype != USER_UNICHAR_TYPE)
			break;
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
	default:
		break;
	}

#ifdef WORDS_BIGENDIAN
	if (tds->conn->emul_little_endian) {
		tdsdump_log(TDS_DBG_INFO1, "swapping coltype %d\n", tds_get_conversion_type(curcol->column_type, colsize));
		tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), dest);
	}
#endif
	return TDS_SUCCESS;
}

/**
 * Put data information to wire
 * \param tds   state information for the socket and the TDS protocol
 * \param col   column where to store information
 * \return TDS_SUCCESS or TDS_FAIL
 */
TDSRET
tds_generic_put_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	size_t size;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(col);

	size = tds_fix_column_size(tds, col);
	switch (col->column_varint_size) {
	case 0:
		break;
	case 1:
		tds_put_byte(tds, size);
		break;
	case 2:
		tds_put_smallint(tds, size);
		break;
	case 5:
	case 4:
		tds_put_int(tds, size);
		break;
	case 8:
		tds_put_smallint(tds, 0xffff);
		break;
	}

	/* TDS5 wants a table name for LOBs */
	if (IS_TDS50(tds->conn) && is_blob_type(col->on_server.column_type))
		tds_put_smallint(tds, 0);

	/* TDS7.1 output collate information */
	if (IS_TDS71_PLUS(tds->conn) && is_collate_type(col->on_server.column_type))
		tds_put_n(tds, tds->conn->collation, 5);

	return TDS_SUCCESS;
}

unsigned
tds_generic_put_info_len(TDSSOCKET * tds, TDSCOLUMN * col)
{
	unsigned len = col->column_varint_size;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(col);

	switch (col->column_varint_size) {
	case 5:
		len = 4;
		break;
	case 8:
		len = 2;
		break;
	}

	if (IS_TDS50(tds->conn)
	    && (col->on_server.column_type == SYBIMAGE || col->on_server.column_type == SYBTEXT))
		len += 2;

	/* TDS7.1 output collate information */
	if (IS_TDS71_PLUS(tds->conn) && is_collate_type(col->on_server.column_type))
		len += 5;

	return len;
}

/**
 * Write data to wire
 * \param tds state information for the socket and the TDS protocol
 * \param curcol column where store column information
 * \return TDS_FAIL on error or TDS_SUCCESS
 */
TDSRET
tds_generic_put(TDSSOCKET * tds, TDSCOLUMN * curcol, int bcp7)
{
	unsigned char *src;
	TDSBLOB *blob = NULL;
	size_t colsize, size;

	const char *s;
	int converted = 0;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(curcol);

	tdsdump_log(TDS_DBG_INFO1, "tds_generic_put: colsize = %d\n", (int) curcol->column_cur_size);

	/* output NULL data */
	if (curcol->column_cur_size < 0) {
		tdsdump_log(TDS_DBG_INFO1, "tds_generic_put: null param\n");
		switch (curcol->column_varint_size) {
		case 5:
			tds_put_int(tds, 0);
			break;
		case 4:
			if ((bcp7 || !IS_TDS7_PLUS(tds->conn)) && is_blob_type(curcol->on_server.column_type))
				tds_put_byte(tds, 0);
			else
				tds_put_int(tds, -1);
			break;
		case 2:
			tds_put_smallint(tds, -1);
			break;
		case 8:
			tds_put_int8(tds, -1);
			break;
		default:
			assert(curcol->column_varint_size);
			/* FIXME not good for SYBLONGBINARY/SYBLONGCHAR (still not supported) */
			tds_put_byte(tds, 0);
			break;
		}
		return TDS_SUCCESS;
	}
	colsize = curcol->column_cur_size;

	size = tds_fix_column_size(tds, curcol);

	src = curcol->column_data;
	if (is_blob_col(curcol)) {
		blob = (TDSBLOB *) src;
		src = (unsigned char *) blob->textvalue;
	}

	s = (char *) src;

	/* convert string if needed */
	if (!bcp7 && curcol->char_conv && curcol->char_conv->flags != TDS_ENCODING_MEMCPY && colsize) {
		size_t output_size;
#if 0
		/* TODO this case should be optimized */
		/* we know converted bytes */
		if (curcol->char_conv->client_charset.min_bytes_per_char == curcol->char_conv->client_charset.max_bytes_per_char 
		    && curcol->char_conv->server_charset.min_bytes_per_char == curcol->char_conv->server_charset.max_bytes_per_char) {
			converted_size = colsize * curcol->char_conv->server_charset.min_bytes_per_char / curcol->char_conv->client_charset.min_bytes_per_char;

		} else {
#endif
		/* we need to convert data before */
		/* TODO this can be a waste of memory... */
		converted = 1;
		s = tds_convert_string(tds, curcol->char_conv, s, colsize, &output_size);
		colsize = (TDS_INT)output_size;
		if (!s) {
			/* on conversion error put a empty string */
			/* TODO on memory failure we should compute converted size and use chunks */
			colsize = 0;
			converted = -1;
		}
	}

	/*
	 * TODO here we limit data sent with MIN, should mark somewhere
	 * and inform client ??
	 * Test proprietary behavior
	 */
	if (IS_TDS7_PLUS(tds->conn)) {
		tdsdump_log(TDS_DBG_INFO1, "tds_generic_put: not null param varint_size = %d\n",
			    curcol->column_varint_size);

		switch (curcol->column_varint_size) {
		case 8:
			/* this difference for BCP operation is due to
			 * a bug in different server version that does
			 * not accept a length here */
			tds_put_int8(tds, bcp7 ? -2 : colsize);
			tds_put_int(tds, colsize);
			break;
		case 4:	/* It's a BLOB... */
			colsize = MIN(colsize, size);
			/* mssql require only size */
			if (bcp7 && is_blob_type(curcol->on_server.column_type)) {
				static const unsigned char textptr[] = {
					0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
				};
				tds_put_byte(tds, 16);
				tds_put_n(tds, textptr, 16);
				tds_put_n(tds, textptr, 8);
			}
			tds_put_int(tds, colsize);
			break;
		case 2:
			colsize = MIN(colsize, size);
			tds_put_smallint(tds, colsize);
			break;
		case 1:
			colsize = MIN(colsize, size);
			tds_put_byte(tds, colsize);
			break;
		case 0:
			/* TODO should be column_size */
			colsize = tds_get_size_by_type(curcol->on_server.column_type);
			break;
		}

		/* conversion error, exit with an error */
		if (converted < 0)
			return TDS_FAIL;

		/* put real data */
		if (blob) {
			tds_put_n(tds, s, colsize);
		} else {
#ifdef WORDS_BIGENDIAN
			unsigned char buf[64];

			if (tds->conn->emul_little_endian && !converted && colsize < 64) {
				tdsdump_log(TDS_DBG_INFO1, "swapping coltype %d\n",
					    tds_get_conversion_type(curcol->column_type, colsize));
				memcpy(buf, s, colsize);
				tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), buf);
				s = (char *) buf;
			}
#endif
			tds_put_n(tds, s, colsize);
		}
		/* finish chunk for varchar/varbinary(max) */
		if (curcol->column_varint_size == 8 && colsize)
			tds_put_int(tds, 0);
	} else {
		/* TODO ICONV handle charset conversions for data */
		/* put size of data */
		switch (curcol->column_varint_size) {
		case 5:	/* It's a LONGBINARY */
			colsize = MIN(colsize, 0x7fffffff);
			tds_put_int(tds, colsize);
			break;
		case 4:	/* It's a BLOB... */
			tds_put_byte(tds, 16);
			tds_put_n(tds, blob->textptr, 16);
			tds_put_n(tds, blob->timestamp, 8);
			colsize = MIN(colsize, 0x7fffffff);
			tds_put_int(tds, colsize);
			break;
		case 2:
			colsize = MIN(colsize, 8000);
			tds_put_smallint(tds, colsize);
			break;
		case 1:
			if (!colsize) {
				tds_put_byte(tds, 1);
				if (is_char_type(curcol->column_type))
					tds_put_byte(tds, ' ');
				else
					tds_put_byte(tds, 0);
				if (converted > 0)
					tds_convert_string_free((char*)src, s);
				return TDS_SUCCESS;
			}
			colsize = MIN(colsize, 255);
			tds_put_byte(tds, colsize);
			break;
		case 0:
			/* TODO should be column_size */
			colsize = tds_get_size_by_type(curcol->column_type);
			break;
		}

		/* conversion error, exit with an error */
		if (converted < 0)
			return TDS_FAIL;

		/* put real data */
		if (blob) {
			tds_put_n(tds, s, colsize);
		} else {
#ifdef WORDS_BIGENDIAN
			unsigned char buf[64];

			if (tds->conn->emul_little_endian && !converted && colsize < 64) {
				tdsdump_log(TDS_DBG_INFO1, "swapping coltype %d\n",
					    tds_get_conversion_type(curcol->column_type, colsize));
				memcpy(buf, s, colsize);
				tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), buf);
				s = (char *) buf;
			}
#endif
			tds_put_n(tds, s, colsize);
		}
	}
	if (converted > 0)
		tds_convert_string_free((char*)src, s);
	return TDS_SUCCESS;
}

TDSRET
tds_numeric_get_info(TDSSOCKET *tds, TDSCOLUMN *col)
{
	col->column_size = tds_get_byte(tds);
	col->column_prec = tds_get_byte(tds);        /* precision */
	col->column_scale = tds_get_byte(tds);       /* scale */

	/* check prec/scale, don't let server crash us */
	if (col->column_prec < 1 || col->column_prec > MAXPRECISION
	    || col->column_scale > col->column_prec)
		return TDS_FAIL;

	return TDS_SUCCESS;
}

TDS_INT
tds_numeric_row_len(TDSCOLUMN *col)
{
	return sizeof(TDS_NUMERIC);
}

TDSRET
tds_numeric_get(TDSSOCKET * tds, TDSCOLUMN * curcol)
{
	int colsize;
	TDS_NUMERIC *num;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(curcol);

	colsize = tds_get_byte(tds);

	/* set NULL flag in the row buffer */
	if (colsize <= 0) {
		curcol->column_cur_size = -1;
		return TDS_SUCCESS;
	}

	/* 
	 * Since these can be passed around independent
	 * of the original column they came from, we embed the TDS_NUMERIC datatype in the row buffer
	 * instead of using the wire representation, even though it uses a few more bytes.  
	 */
	num = (TDS_NUMERIC *) curcol->column_data;
	memset(num, '\0', sizeof(TDS_NUMERIC));
	/* TODO perhaps it would be fine to change format ?? */
	num->precision = curcol->column_prec;
	num->scale = curcol->column_scale;

	/* server is going to crash freetds ?? */
	/* TODO close connection it server try to do so ?? */
	if (colsize > sizeof(num->array))
		return TDS_FAIL;
	tds_get_n(tds, num->array, colsize);

	if (IS_TDS7_PLUS(tds->conn))
		tds_swap_numeric(num);

	/* corrected colsize for column_cur_size */
	curcol->column_cur_size = sizeof(TDS_NUMERIC);

	return TDS_SUCCESS;
}

TDSRET
tds_numeric_put_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(col);

#if 1
	tds_put_byte(tds, tds_numeric_bytes_per_prec[col->column_prec]);
	tds_put_byte(tds, col->column_prec);
	tds_put_byte(tds, col->column_scale);
#else
	TDS_NUMERIC *num = (TDS_NUMERIC *) col->column_data;
	tds_put_byte(tds, tds_numeric_bytes_per_prec[num->precision]);
	tds_put_byte(tds, num->precision);
	tds_put_byte(tds, num->scale);
#endif

	return TDS_SUCCESS;
}

unsigned
tds_numeric_put_info_len(TDSSOCKET * tds, TDSCOLUMN * col)
{
	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(col);

	return 3;
}

TDSRET
tds_numeric_put(TDSSOCKET *tds, TDSCOLUMN *col, int bcp7)
{
	TDS_NUMERIC *num = (TDS_NUMERIC *) col->column_data, buf;
	unsigned char colsize;

	if (col->column_cur_size < 0) {
		tds_put_byte(tds, 0);
		return TDS_SUCCESS;
	}
	colsize = tds_numeric_bytes_per_prec[num->precision];
	tds_put_byte(tds, colsize);

	buf = *num;
	if (IS_TDS7_PLUS(tds->conn))
		tds_swap_numeric(&buf);
	tds_put_n(tds, buf.array, colsize);
	return TDS_SUCCESS;
}

TDSRET
tds_variant_put_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	/* TODO */
	return TDS_FAIL;
}

TDSRET
tds_variant_put(TDSSOCKET *tds, TDSCOLUMN *col, int bcp7)
{
	/* TODO */
	return TDS_FAIL;
}

TDSRET
tds_msdatetime_get_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	col->column_scale = col->column_prec = 0;
	if (col->column_type != SYBMSDATE) {
		col->column_scale = col->column_prec = tds_get_byte(tds);
		if (col->column_prec > 7)
			return TDS_FAIL;
	}
	col->on_server.column_size = col->column_size = sizeof(TDS_DATETIMEALL);
	return TDS_SUCCESS;
}

TDS_INT
tds_msdatetime_row_len(TDSCOLUMN *col)
{
	return sizeof(TDS_DATETIMEALL);
}

TDSRET
tds_msdatetime_get(TDSSOCKET * tds, TDSCOLUMN * col)
{
	TDS_DATETIMEALL *dt = (TDS_DATETIMEALL*) col->column_data;
	int size = tds_get_byte(tds);

	if (size == 0) {
		col->column_cur_size = -1;
		return TDS_SUCCESS;
	}

	memset(dt, 0, sizeof(*dt));

	if (col->column_type == SYBMSDATETIMEOFFSET)
		size -= 2;
	if (col->column_type != SYBMSTIME)
		size -= 3;
	if (size < 0)
		return TDS_FAIL;

	dt->time_prec = col->column_prec;

	/* get time part */
	if (col->column_type != SYBMSDATE) {
		TDS_UINT8 u8;
		int i;

		if (size < 3 || size > 5)
			return TDS_FAIL;
		u8 = 0;
		tds_get_n(tds, &u8, size);
#ifdef WORDS_BIGENDIAN
		tds_swap_bytes(&u8, 8);
#endif
		for (i = col->column_prec; i < 7; ++i)
			u8 *= 10;
		dt->time = u8;
		dt->has_time = 1;
	} else if (size != 0)
		return TDS_FAIL;

	/* get date part */
	if (col->column_type != SYBMSTIME) {
		TDS_UINT ui;

		ui = 0;
		tds_get_n(tds, &ui, 3);
#ifdef WORDS_BIGENDIAN
		tds_swap_bytes(&ui, 4);
#endif
		dt->has_date = 1;
		dt->date = ui - 693595;
	}

	/* get time offset */
	if (col->column_type == SYBMSDATETIMEOFFSET) {
		dt->offset = tds_get_smallint(tds);
		if (dt->offset > 840 || dt->offset < -840)
			return TDS_FAIL;
		dt->has_offset = 1;
	}
	col->column_cur_size = sizeof(TDS_DATETIMEALL);
	return TDS_SUCCESS;
}

TDSRET
tds_msdatetime_put_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	/* TODO precision */
	if (col->on_server.column_type != SYBMSDATE)
		tds_put_byte(tds, 7);
	return TDS_SUCCESS;
}

TDSRET
tds_msdatetime_put(TDSSOCKET *tds, TDSCOLUMN *col, int bcp7)
{
	const TDS_DATETIMEALL *dta = (const TDS_DATETIMEALL *) col->column_data;
	unsigned char buf[12], *p;

	if (col->column_cur_size < 0) {
		tds_put_byte(tds, 0);
		return TDS_SUCCESS;
	}

	/* TODO precision */
	p = buf + 1;
	if (col->on_server.column_type != SYBMSDATE) {
		TDS_PUT_UA4LE(p, (TDS_UINT) dta->time);
		p[4] = (unsigned char) (dta->time >> 32);
		p += 5;
	}
	if (col->on_server.column_type != SYBMSTIME) {
		TDS_UINT ui = dta->date + 693595;
		TDS_PUT_UA4LE(p, ui);
		p += 3;
	}
	if (col->on_server.column_type == SYBMSDATETIMEOFFSET) {
		TDS_PUT_UA2LE(p, dta->offset);
		p += 2;
	}
	buf[0] = p - buf - 1;
	tds_put_n(tds, buf, p - buf);

	return TDS_SUCCESS;
}

TDSRET
tds_clrudt_get_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	/* TODO save fields */
	/* FIXME support RPC */

	/* MAX_BYTE_SIZE */
	tds_get_usmallint(tds);

	/* DB_NAME */
	tds_get_string(tds, tds_get_byte(tds), NULL, 0);

	/* SCHEMA_NAME */
	tds_get_string(tds, tds_get_byte(tds), NULL, 0);

	/* TYPE_NAME */
	tds_get_string(tds, tds_get_byte(tds), NULL, 0);

	/* UDT_METADATA */
	tds_get_string(tds, tds_get_usmallint(tds), NULL, 0);

	col->column_size = 0x7ffffffflu;
	col->column_varint_size = 8;

	return TDS_SUCCESS;
}

TDS_INT
tds_clrudt_row_len(TDSCOLUMN *col)
{
	col->column_varint_size = 8;
	/* TODO save other fields */
	return sizeof(TDSBLOB);
}

unsigned
tds_clrudt_put_info_len(TDSSOCKET * tds, TDSCOLUMN * col)
{
	return 3;
}

TDSRET
tds_clrudt_put_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	/* FIXME support properly*/
	tds_put_byte(tds, 0);	/* db_name */
	tds_put_byte(tds, 0);	/* schema_name */
	tds_put_byte(tds, 0);	/* type_name */

	return TDS_SUCCESS;
}

TDSRET
tds_sybbigtime_get_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	col->column_scale = col->column_prec = 6;
	tds_get_byte(tds); /* 8, size */
	tds_get_byte(tds); /* 6, precision ?? */
	col->on_server.column_size = col->column_size = sizeof(TDS_UINT8);
	return TDS_SUCCESS;
}

TDS_INT
tds_sybbigtime_row_len(TDSCOLUMN *col)
{
	return sizeof(TDS_UINT8);
}

TDSRET
tds_sybbigtime_get(TDSSOCKET * tds, TDSCOLUMN * col)
{
	TDS_UINT8 *dt = (TDS_UINT8 *) col->column_data;
	int size = tds_get_byte(tds);

	if (size == 0) {
		col->column_cur_size = -1;
		return TDS_SUCCESS;
	}

	col->column_cur_size = sizeof(TDS_UINT8);
	*dt = tds_get_int8(tds);

	return TDS_SUCCESS;
}

TDSRET
tds_sybbigtime_put_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	tds_put_byte(tds, 8);
	tds_put_byte(tds, 6);
	return TDS_SUCCESS;
}

unsigned
tds_sybbigtime_put_info_len(TDSSOCKET * tds, TDSCOLUMN * col)
{
	return 2;
}

TDSRET
tds_sybbigtime_put(TDSSOCKET *tds, TDSCOLUMN *col, int bcp7)
{
	const TDS_UINT8 *dt = (const TDS_UINT8 *) col->column_data;

	if (col->column_cur_size < 0) {
		tds_put_byte(tds, 0);
		return TDS_SUCCESS;
	}

	tds_put_byte(tds, 8);
	tds_put_int8(tds, *dt);

	return TDS_SUCCESS;
}

TDSRET
tds_invalid_get_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	return TDS_FAIL;
}

TDS_INT
tds_invalid_row_len(TDSCOLUMN *col)
{
	return 0;
}

TDSRET
tds_invalid_get(TDSSOCKET * tds, TDSCOLUMN * col)
{
	return TDS_FAIL;
}

TDSRET
tds_invalid_put_info(TDSSOCKET * tds, TDSCOLUMN * col)
{
	return TDS_FAIL;
}

unsigned
tds_invalid_put_info_len(TDSSOCKET * tds, TDSCOLUMN * col)
{
	return 0;
}

TDSRET
tds_invalid_put(TDSSOCKET *tds, TDSCOLUMN *col, int bcp7)
{
	return TDS_FAIL;
}

#if ENABLE_EXTRA_CHECKS
int
tds_generic_check(const TDSCOLUMN *col)
{
	return 0;
}

int
tds_sybbigtime_check(const TDSCOLUMN *col)
{
	assert(col->column_type == col->on_server.column_type);
	assert(col->on_server.column_size == col->column_size);
	assert(!is_numeric_type(col->column_type));
	assert(!is_fixed_type(col->column_type));
	assert(!is_blob_type(col->column_type));
	assert(!is_variable_type(col->column_type));
	assert(is_nullable_type(col->column_type));
	assert(col->column_varint_size == 1);
	assert(col->column_prec == 6);
	assert(col->column_scale == col->column_prec);

	return 1;
}

int
tds_clrudt_check(const TDSCOLUMN *col)
{
	return 0;
}

int
tds_msdatetime_check(const TDSCOLUMN *col)
{
	assert(col->column_type == col->on_server.column_type);
	assert(col->on_server.column_size == col->column_size);
	assert(!is_numeric_type(col->column_type));
	if (col->column_type == SYBMSDATE) {
		assert(is_fixed_type(col->column_type));
	} else {
		assert(!is_fixed_type(col->column_type));
	}
	assert(!is_blob_type(col->column_type));
	assert(!is_variable_type(col->column_type));
	assert(is_nullable_type(col->column_type));
	assert(col->column_varint_size == 1);
	assert(col->column_prec >= 0 && col->column_prec <= 7);
	assert(col->column_scale == col->column_prec);

	return 1;
}

int
tds_variant_check(const TDSCOLUMN *col)
{
	return 0;
}

int
tds_numeric_check(const TDSCOLUMN *col)
{
	assert(col->column_type == col->on_server.column_type);
	assert(col->on_server.column_size == col->column_size);
	assert(is_numeric_type(col->column_type));
	assert(!is_fixed_type(col->column_type));
	assert(!is_blob_type(col->column_type));
	assert(!is_variable_type(col->column_type));
	assert(col->column_varint_size == 1);
	assert(col->column_prec >= 1 && col->column_prec <= MAXPRECISION);
	assert(col->column_scale <= col->column_prec);

	return 1;
}

int
tds_invalid_check(const TDSCOLUMN *col)
{
	return 1;
}
#endif


#define TDS_DECLARE_FUNCS(name) \
     extern const TDSCOLUMNFUNCS tds_ ## name ## _funcs

#include <freetds/pushvis.h>
TDS_DECLARE_FUNCS(generic);
TDS_DECLARE_FUNCS(numeric);
TDS_DECLARE_FUNCS(variant);
TDS_DECLARE_FUNCS(msdatetime);
TDS_DECLARE_FUNCS(clrudt);
TDS_DECLARE_FUNCS(sybbigtime);
TDS_DECLARE_FUNCS(invalid);
#include <freetds/popvis.h>

static const TDSCOLUMNFUNCS *
tds_get_column_funcs(TDSCONNECTION *conn, int type)
{
	switch (type) {
	case SYBNUMERIC:
	case SYBDECIMAL:
		return &tds_numeric_funcs;
	case SYBMSUDT:
		return &tds_clrudt_funcs;
	case SYBVARIANT:
		if (IS_TDS7_PLUS(conn))
			return &tds_variant_funcs;
		break;
	case SYBMSDATE:
	case SYBMSTIME:
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
		return &tds_msdatetime_funcs;
	case SYB5BIGTIME:
	case SYB5BIGDATETIME:
		return &tds_sybbigtime_funcs;
	}
	return &tds_generic_funcs;
}
#include "tds_types.h"

#ifdef WORDS_BIGENDIAN
static void
tds_swap_datatype(int coltype, void *b)
{
	unsigned char *buf = (unsigned char *) b;

	switch (coltype) {
	case SYBDATETIME4:
		tds_swap_bytes(&buf[2], 2);
	case SYBINT2:
		tds_swap_bytes(buf, 2);
		break;
	case SYBMONEY:
	case SYBDATETIME:
		tds_swap_bytes(&buf[4], 4);
	case SYBINT4:
	case SYBMONEY4:
	case SYBREAL:
	case SYBDATE:
	case SYBTIME:
		tds_swap_bytes(buf, 4);
		break;
	case SYBINT8:
	case SYBFLT8:
	case SYB5BIGTIME:
	case SYB5BIGDATETIME:
		tds_swap_bytes(buf, 8);
		break;
	case SYBUNIQUE:
		tds_swap_bytes(buf, 4);
		tds_swap_bytes(&buf[4], 2);
		tds_swap_bytes(&buf[6], 2);
		break;
	}
}
#endif

/**
 * Converts numeric from Microsoft representation to internal one (Sybase).
 * \param num numeric data to convert
 */
static void
tds_swap_numeric(TDS_NUMERIC *num)
{
	/* swap the sign */
	num->array[0] = (num->array[0] == 0) ? 1 : 0;
	/* swap the data */
	tds_swap_bytes(&(num->array[1]), tds_numeric_bytes_per_prec[num->precision] - 1);
}

