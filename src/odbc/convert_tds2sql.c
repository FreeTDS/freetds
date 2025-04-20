/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2003-2010  Frediano Ziglio
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

#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <ctype.h>

#include <freetds/utils.h>
#include <freetds/odbc.h>
#include <freetds/convert.h>
#include <freetds/iconv.h>
#include <freetds/utils/string.h>
#include <freetds/encodings.h>
#include <odbcss.h>

#define TDS_ISSPACE(c) isspace((unsigned char) (c))

/**
 * Copy beginning of column_iconv_buf
 */
static void
eat_iconv_left(TDSCOLUMN * curcol, char **pbuf, size_t *plen)
{
	unsigned cp = (unsigned) TDS_MIN(*plen, curcol->column_iconv_left);
	memcpy(*pbuf, curcol->column_iconv_buf, cp);
	if (cp < curcol->column_iconv_left)
		memmove(curcol->column_iconv_buf, curcol->column_iconv_buf + cp, curcol->column_iconv_left - cp);
	curcol->column_iconv_left -= cp;
	*pbuf += cp;
	*plen -= cp;
}

/**
 * Handle conversions from TDS (N)CHAR to ODBC (W)CHAR
 */
static SQLLEN
odbc_convert_char(TDS_STMT * stmt, TDSCOLUMN * curcol, TDS_CHAR * src, TDS_UINT srclen,
		  int desttype, TDS_CHAR * dest, SQLULEN destlen)
{
	const char *ib;
	char *ob;
	size_t il, ol, char_size;

	/* FIXME MARS not correct cause is the global tds but stmt->tds can be NULL on SQLGetData */
	TDSSOCKET *tds = stmt->dbc->tds_socket;

	TDSICONV *conv = curcol->char_conv;
	if (!conv)
		conv = tds->conn->char_convs[client2server_chardata];
	if (desttype == SQL_C_WCHAR) {
		int charset = odbc_get_wide_canonic(tds->conn);
		/* SQL_C_WCHAR, convert to wide encode */
		conv = tds_iconv_get_info(tds->conn, charset, conv->to.charset.canonic);
		if (!conv)
			conv = tds_iconv_get_info(tds->conn, charset, TDS_CHARSET_ISO_8859_1);
#ifdef ENABLE_ODBC_WIDE
	} else {
		conv = tds_iconv_get_info(tds->conn, stmt->dbc->original_charset_num, conv->to.charset.canonic);
		if (!conv)
			conv = tds_iconv_get_info(tds->conn, stmt->dbc->original_charset_num, TDS_CHARSET_ISO_8859_1);
		if (!conv)
			conv = tds_iconv_get_info(tds->conn, TDS_CHARSET_ISO_8859_1, TDS_CHARSET_ISO_8859_1);
#endif
	}

	ib = src;
	il = srclen;
	ob = dest;
	ol = 0;
	char_size = desttype == SQL_C_CHAR ? 1 : SIZEOF_SQLWCHAR;
	if (destlen >= char_size) {
		ol = destlen - char_size;
		/* copy left and continue only if possible */
		eat_iconv_left(curcol, &ob, &ol);
		if (ol) {
			memset(&conv->suppress, 0, sizeof(conv->suppress));
			conv->suppress.eilseq = 1;
			conv->suppress.e2big = 1;
			/* TODO check return value */
			tds_iconv(tds, conv, to_client, &ib, &il, &ob, &ol);
		}
		/* if input left try to decode on future left */
		if (il && ol < sizeof(curcol->column_iconv_buf) && curcol->column_iconv_left == 0) {
			char *left_ob = curcol->column_iconv_buf;
			size_t left_ol = sizeof(curcol->column_iconv_buf);
			conv->suppress.eilseq = 1;
			conv->suppress.einval = 1;
			conv->suppress.e2big = 1;
			tds_iconv(tds, conv, to_client, &ib, &il, &left_ob, &left_ol);
			curcol->column_iconv_left = sizeof(curcol->column_iconv_buf) - (unsigned char) left_ol;
			/* copy part to fill buffer */
			eat_iconv_left(curcol, &ob, &ol);
		}
		ol = ob - dest; /* bytes written */
		curcol->column_text_sqlgetdatapos += (TDS_INT) (ib - src);
		/* terminate string */
		memset(ob, 0, char_size);
	}

	/* returned size have to take into account buffer left unconverted */
	if (il == 0 || (conv->from.charset.min_bytes_per_char == conv->from.charset.max_bytes_per_char
	    && conv->to.charset.min_bytes_per_char == conv->to.charset.max_bytes_per_char)) {
		ol += il * conv->from.charset.min_bytes_per_char / conv->to.charset.min_bytes_per_char + curcol->column_iconv_left;
	} else if ((conv->flags & TDS_ENCODING_MEMCPY) != 0) {
		ol += il + curcol->column_iconv_left;
	} else {
		/* TODO convert and discard ?? or return proper SQL_NO_TOTAL values ?? */
		return SQL_NO_TOTAL;
	}
	return ol;
}

/**
 * Handle conversions from TDS NCHAR to ISO8859-1 stripping spaces (for fixed types)
 */
static int
odbc_tds_convert_wide_iso(TDS_CHAR *src, TDS_UINT srclen, TDS_CHAR *buf, TDS_UINT buf_len)
{
	TDS_CHAR *p;
	/*
	 * TODO check for endian
	 * This affect for instance Sybase under little endian system
	 */
	
	/* skip white spaces */
	while (srclen > 1 && src[1] == 0 && TDS_ISSPACE(src[0])) {
		srclen -= 2;
		src += 2;
	}

	/* convert */
	p = buf;
	while (buf_len > 1 && srclen > 1 && src[1] == 0) {
		*p++ = src[0];
		--buf_len;
		srclen -= 2;
		src += 2;
	}

	/* skip white spaces */
	while (srclen > 1 && src[1] == 0 && TDS_ISSPACE(src[0])) {
		srclen -= 2;
		src += 2;
	}

	/* still characters, wrong format */
	if (srclen)
		return -1;

	*p = 0;
	return (int) (p - buf);
}

/* The following function is going to write in these structure not using them
 * but just knowing the ABI. Check these ABI. Mainly make sure the alignment
 * is still correct.
 */
TDS_COMPILE_CHECK(ss_time2, sizeof(SQL_SS_TIME2_STRUCT) == 12
	&& TDS_OFFSET(SQL_SS_TIME2_STRUCT, fraction) == 8);
TDS_COMPILE_CHECK(ss_timestampoffset, sizeof(SQL_SS_TIMESTAMPOFFSET_STRUCT) == 20
	&& TDS_OFFSET(SQL_SS_TIMESTAMPOFFSET_STRUCT, fraction) == 12);
TDS_COMPILE_CHECK(date_struct, sizeof(DATE_STRUCT) == 6
	&& TDS_OFFSET(DATE_STRUCT, year) == 0
	&& TDS_OFFSET(DATE_STRUCT, month) == 2
	&& TDS_OFFSET(DATE_STRUCT, day) == 4);
TDS_COMPILE_CHECK(timestamp_struct, sizeof(TIMESTAMP_STRUCT) == 16
	&& TDS_OFFSET(TIMESTAMP_STRUCT, year) == 0
	&& TDS_OFFSET(TIMESTAMP_STRUCT, month) == 2
	&& TDS_OFFSET(TIMESTAMP_STRUCT, day) == 4
	&& TDS_OFFSET(TIMESTAMP_STRUCT, hour) == 6
	&& TDS_OFFSET(TIMESTAMP_STRUCT, minute) == 8
	&& TDS_OFFSET(TIMESTAMP_STRUCT, second) == 10
	&& TDS_OFFSET(TIMESTAMP_STRUCT, fraction) == 12);

/**
 * Handle conversions from MSSQL 2008 DATE/TIME types to binary.
 * These types have a different binary representation in libTDS.
 */
static SQLLEN
odbc_convert_datetime_to_binary(TDSCOLUMN *curcol, int srctype, TDS_DATETIMEALL * dta, TDS_CHAR * dest, SQLULEN destlen)
{
	TDS_INT len, cplen;
	TDS_USMALLINT buf[10];
	TDSDATEREC when;

	tds_datecrack(srctype, dta, &when);

	len = 0;
	if (srctype != SYBMSTIME && srctype != SYBTIME && srctype != SYB5BIGTIME) {
		buf[0] = when.year;
		buf[1] = when.month + 1;
		buf[2] = when.day;
		len = 3;
	}
	if (srctype != SYBMSDATE && srctype != SYBDATE) {
		buf[len++] = when.hour;
		buf[len++] = when.minute;
		buf[len++] = when.second;
		if ((len % 2) != 0)
			buf[len++] = 0;
		*((TDS_UINT*) (buf+len)) = when.decimicrosecond * 100u;
		len += 2;
	}
	if (srctype == SYBMSDATETIMEOFFSET) {
		/* TODO check for negative hour/minutes */
		buf[8] = dta->offset / 60;
		buf[9] = dta->offset % 60;
		len = 10;
	}
	len *= 2;

	/* just return length */
	if (destlen == 0)
		return len;

	cplen = TDS_MIN((TDS_INT) destlen, len);
	memcpy(dest, buf, cplen);
	if (curcol)
		curcol->column_text_sqlgetdatapos += cplen;
	return len;
}

static SQLLEN
odbc_convert_to_binary(TDSCOLUMN *curcol, int srctype, TDS_CHAR * src, TDS_UINT srclen, TDS_CHAR * dest, SQLULEN destlen)
{
	SQLLEN ret = srclen;

	/* special case for date/time */
	switch (srctype) {
	case SYBMSTIME:
	case SYBMSDATE:
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
	case SYBDATE:
	case SYBTIME:
	case SYB5BIGTIME:
	case SYB5BIGDATETIME:
		return odbc_convert_datetime_to_binary(curcol, srctype, (TDS_DATETIMEALL *) src, dest, destlen);
	}

	/* if destlen == 0 we return only length */
	if (destlen > 0) {
		size_t cplen = TDS_MIN(destlen, srclen);
		/* do not NUL terminate binary buffer */
		memcpy(dest, src, cplen);
		if (curcol)
			curcol->column_text_sqlgetdatapos += cplen;
	}
	return ret;
}

static SQLLEN
odbc_tds2sql(TDS_STMT * stmt, TDSCOLUMN *curcol, int srctype, TDS_CHAR * src, TDS_UINT srclen,
	     int desttype, TDS_CHAR * dest, SQLULEN destlen,
	     const struct _drecord *drec_ixd)
{
	TDS_INT nDestSybType;
	TDS_INT nRetVal = TDS_CONVERT_FAIL;
	TDSCONTEXT *context = stmt->dbc->env->tds_ctx;

	CONV_RESULT ores;

	SQLLEN ret = SQL_NULL_DATA;
	int i;
	SQLULEN cplen;
	int binary_conversion = 0;
	TDS_CHAR conv_buf[256];

	tdsdump_log(TDS_DBG_FUNC, "odbc_tds2sql: src is %d dest = %d\n", srctype, desttype);

	assert(desttype != SQL_C_DEFAULT);

	nDestSybType = odbc_c_to_server_type(desttype);
	if (!nDestSybType) {
		odbc_errs_add(&stmt->errs, "HY003", NULL);
		return SQL_NULL_DATA;
	}

	/* special case for binary type */
	if (desttype == SQL_C_BINARY) {
		tdsdump_log(TDS_DBG_FUNC, "odbc_tds2sql: outputting binary data destlen = %lu \n", (unsigned long) destlen);

		if (is_numeric_type(srctype)) {
			desttype = SQL_C_NUMERIC;
			nDestSybType = SYBNUMERIC;
			/* prevent buffer overflow */
			if (destlen < sizeof(SQL_NUMERIC_STRUCT)) {
				odbc_errs_add(&stmt->errs, "07006", NULL);
				return SQL_NULL_DATA;
			}
			ores.n.precision = ((TDS_NUMERIC *) src)->precision;
			ores.n.scale = ((TDS_NUMERIC *) src)->scale;
		} else {
			return odbc_convert_to_binary(curcol, srctype, src, srclen, dest, destlen);
		}
	} else if (is_numeric_type(nDestSybType)) {
		/* TODO use descriptor information (APD) ?? However APD can contain SQL_C_DEFAULT... */
		if (drec_ixd)
			ores.n.precision = (unsigned char) drec_ixd->sql_desc_precision;
		else
			ores.n.precision = 38;
		ores.n.scale = 0;
	}

	if (is_char_type(srctype)) {
		if (desttype == SQL_C_CHAR || desttype == SQL_C_WCHAR)
			return odbc_convert_char(stmt, curcol, src, srclen, desttype, dest, destlen);
		if (is_unicode_type(srctype)) {
			/*
			 * convert to single and then process normally.
			 * Here we processed SQL_C_BINARY and SQL_C_*CHAR so only fixed types are left
			 */
			i = odbc_tds_convert_wide_iso(src, srclen, conv_buf, sizeof(conv_buf));
			if (i < 0)
				return SQL_NULL_DATA;
			src = conv_buf;
			srclen = i;
			srctype = SYBVARCHAR;
		}
	}

	if (desttype == SQL_C_WCHAR)
		destlen /= sizeof(SQLWCHAR);
	if (desttype == SQL_C_CHAR || desttype == SQL_C_WCHAR) {
		if (is_binary_type(srctype)) {
			binary_conversion = 1;
			if (destlen && !(destlen % 2))
				--destlen;
		}

		nDestSybType = TDS_CONVERT_CHAR;
		ores.cc.len = destlen;
		ores.cc.c = dest;
	}

	if (desttype == SQL_C_CHAR || desttype == SQL_C_WCHAR) {
		char buf[48];
		TDSDATEREC when;
		int prec;
		const char *fmt = NULL;
		const TDS_DATETIMEALL *dta = (const TDS_DATETIMEALL *) src;

		switch (srctype) {
		case SYBMSDATETIMEOFFSET:
		case SYBMSDATETIME2:
			prec = dta->time_prec;
			goto datetime;
		case SYB5BIGDATETIME:
			prec = 6;
			goto datetime;
		case SYBDATETIME:
			prec = 3;
			goto datetime;
		case SYBDATETIME4:
			prec = 0;
		datetime:
			fmt = "%Y-%m-%d %H:%M:%S.%z";
			break;
		case SYBMSTIME:
			prec = dta->time_prec;
			goto time;
		case SYB5BIGTIME:
			prec = 6;
			goto time;
		case SYBTIME:
			prec = 3;
		time:
			fmt = "%H:%M:%S.%z";
			break;
		case SYBMSDATE:
		case SYBDATE:
			prec = 0;
			fmt = "%Y-%m-%d";
			break;
		}
		if (!fmt) goto normal_conversion;

		tds_datecrack(srctype, src, &when);
		tds_strftime(buf, sizeof(buf), fmt, &when, prec);

		if (srctype == SYBMSDATETIMEOFFSET) {
			char sign = '+';
			int off = dta->offset;
			if (off < 0) {
				sign = '-';
				off = -off;
			}
			sprintf(buf + strlen(buf), " %c%02d:%02d", sign, off / 60, off % 60);
		}

		nRetVal = (TDS_INT) strlen(buf);
		memcpy(dest, buf, TDS_MIN(destlen, (SQLULEN) nRetVal));
	} else {
normal_conversion:
		nRetVal = tds_convert(context, srctype, src, srclen, nDestSybType, &ores);
	}
	if (nRetVal < 0) {
		odbc_convert_err_set(&stmt->errs, nRetVal);
		return SQL_NULL_DATA;
	}

	switch (desttype) {

	case SQL_C_CHAR:
		tdsdump_log(TDS_DBG_FUNC, "odbc_tds2sql: outputting character data destlen = %lu \n", (unsigned long) destlen);

		ret = nRetVal;
		/* TODO handle not terminated configuration */
		if (destlen > 0) {
			cplen = TDS_MIN(destlen - 1, (SQLULEN) nRetVal);
			assert(cplen >= 0);
			/*
			 * odbc always terminate but do not overwrite 
			 * destination buffer more than needed
			 */
			/* update datapos only for binary source (char already handled) */
			if (curcol && binary_conversion)
				curcol->column_text_sqlgetdatapos += cplen / 2;
			dest[cplen] = 0;
		} else {
			/* if destlen == 0 we return only length */
		}
		break;

	case SQL_C_WCHAR:
		tdsdump_log(TDS_DBG_FUNC, "odbc_tds2sql: outputting character data destlen = %lu \n", (unsigned long) destlen);

		ret = nRetVal * sizeof(SQLWCHAR);
		/* TODO handle not terminated configuration */
		if (destlen > 0) {
			SQLWCHAR *wp = (SQLWCHAR *) dest;
			SQLCHAR  *p  = (SQLCHAR *)  dest;

			cplen = TDS_MIN(destlen - 1, (SQLULEN) nRetVal);
			assert(cplen >= 0);
			/*
			 * odbc always terminate but do not overwrite 
			 * destination buffer more than needed
			 */
			/* update datapos only for binary source (char already handled) */
			if (curcol && binary_conversion)
				curcol->column_text_sqlgetdatapos += cplen / 2;
			/* convert in place and terminate */
			wp[cplen] = 0;
			while (cplen > 0) {
				--cplen;
				wp[cplen] = p[cplen];
			}
		} else {
			/* if destlen == 0 we return only length */
		}
		break;

	case SQL_C_TYPE_DATE:
	case SQL_C_DATE:
		{
			TDSDATEREC dr;
			DATE_STRUCT *dsp = (DATE_STRUCT *) dest;

			/*
			 * we've already converted the returned value to a SYBMSDATETIME2
			 * now decompose date into constituent parts...
			 */
			tds_datecrack(SYBMSDATETIME2, &(ores.dt), &dr);

			dsp->year = dr.year;
			dsp->month = dr.month + 1;
			dsp->day = dr.day;

			ret = sizeof(DATE_STRUCT);
		}
		break;

	case SQL_C_TYPE_TIME:
	case SQL_C_TIME:
		{
			TDSDATEREC dr;
			TIME_STRUCT *tsp = (TIME_STRUCT *) dest;

			/*
			 * we've already converted the returned value to a SYBMSDATETIME2
			 * now decompose date into constituent parts...
			 */
			tds_datecrack(SYBMSDATETIME2, &(ores.dt), &dr);

			tsp->hour = dr.hour;
			tsp->minute = dr.minute;
			tsp->second = dr.second;

			ret = sizeof(TIME_STRUCT);
		}
		break;

	case SQL_C_TYPE_TIMESTAMP:
	case SQL_C_TIMESTAMP: 
		{
			TDSDATEREC dr;
			TIMESTAMP_STRUCT *tssp = (TIMESTAMP_STRUCT *) dest;

			/*
			 * we've already converted the returned value to a SYBMSDATETIME2
			 * now decompose date into constituent parts...
			 */
			tds_datecrack(SYBMSDATETIME2, &(ores.dt), &dr);

			tssp->year = dr.year;
			tssp->month = dr.month + 1;
			tssp->day = dr.day;
			tssp->hour = dr.hour;
			tssp->minute = dr.minute;
			tssp->second = dr.second;
			tssp->fraction = dr.decimicrosecond * 100u;

			ret = sizeof(TIMESTAMP_STRUCT);
		}
		break;

#ifdef SQL_C_SBIGINT
	case SQL_C_SBIGINT:
	case SQL_C_UBIGINT:
		*((TDS_INT8 *) dest) = ores.bi;
		ret = sizeof(TDS_INT8);
		break;
#endif

	case SQL_C_LONG:
	case SQL_C_SLONG:
	case SQL_C_ULONG:
		*((TDS_INT *) dest) = ores.i;
		ret = sizeof(TDS_INT);
		break;

	case SQL_C_SHORT:
	case SQL_C_SSHORT:
	case SQL_C_USHORT:
		*((TDS_SMALLINT *) dest) = ores.si;
		ret = sizeof(TDS_SMALLINT);
		break;

	case SQL_C_TINYINT:
	case SQL_C_STINYINT:
	case SQL_C_UTINYINT:
	case SQL_C_BIT:
		*((TDS_TINYINT *) dest) = ores.ti;
		ret = sizeof(TDS_TINYINT);
		break;

	case SQL_C_DOUBLE:
		*((TDS_FLOAT *) dest) = ores.f;
		ret = sizeof(TDS_FLOAT);
		break;

	case SQL_C_FLOAT:
		*((TDS_REAL *) dest) = ores.r;
		ret = sizeof(TDS_REAL);
		break;

	case SQL_C_NUMERIC:
		{
			/* ODBC numeric is quite different from TDS one ... */
			SQL_NUMERIC_STRUCT *num = (SQL_NUMERIC_STRUCT *) dest;
			size_t len;
			num->precision = ores.n.precision;
			num->scale = ores.n.scale;
			num->sign = ores.n.array[0] ^ 1;
			/*
			 * TODO can be greater than SQL_MAX_NUMERIC_LEN ?? 
			 * Seeing Sybase manual wire supports bigger numeric but current
			 * DBs do not support such precisions.
			 */
			len = TDS_MIN(tds_numeric_bytes_per_prec[ores.n.precision] - 1, SQL_MAX_NUMERIC_LEN);
			memset(num->val, 0, SQL_MAX_NUMERIC_LEN);
			memcpy(num->val, ores.n.array + 1, len);
			tds_swap_bytes(num->val, len);
			ret = sizeof(SQL_NUMERIC_STRUCT);
		}
		break;

#ifdef SQL_C_GUID
	case SQL_C_GUID:
		memcpy(dest, &(ores.u), sizeof(TDS_UNIQUE));
		ret = sizeof(TDS_UNIQUE);
		break;
#endif

	default:
		break;
	}

	return ret;
}

SQLLEN odbc_tds2sql_col(TDS_STMT * stmt, TDSCOLUMN *curcol, int desttype, TDS_CHAR * dest, SQLULEN destlen,
			const struct _drecord *drec_ixd)
{
	int srctype = tds_get_conversion_type(curcol->on_server.column_type, curcol->on_server.column_size);
	TDS_CHAR *src = (TDS_CHAR *) curcol->column_data;
	TDS_UINT srclen = curcol->column_cur_size;

	if (is_blob_col(curcol)) {
		if (srctype == SYBLONGBINARY && (
		    curcol->column_usertype == USER_UNICHAR_TYPE ||
		    curcol->column_usertype == USER_UNIVARCHAR_TYPE))
			srctype = SYBNTEXT;
		if (srctype == SYBVARIANT)
			srctype = ((TDSVARIANT *) src)->type;
		src = ((TDSBLOB *) src)->textvalue;
	}
	if (is_variable_type(srctype)) {
		src += curcol->column_text_sqlgetdatapos;
		srclen -= curcol->column_text_sqlgetdatapos;
	}
	return odbc_tds2sql(stmt, curcol, srctype, src, srclen, desttype, dest, destlen, drec_ixd);
}

SQLLEN odbc_tds2sql_int4(TDS_STMT * stmt, TDS_INT *src, int desttype, TDS_CHAR * dest, SQLULEN destlen)
{
	return odbc_tds2sql(stmt, NULL, SYBINT4, (TDS_CHAR *) src, sizeof(*src),
			    desttype, dest, destlen, NULL);
}
