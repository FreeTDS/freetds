/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2003-2008  Frediano Ziglio
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

#include "tdsodbc.h"
#include "tdsconvert.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: convert_tds2sql.c,v 1.52 2008-09-11 15:09:49 freddy77 Exp $");

TDS_INT
odbc_tds2sql(TDSCONTEXT * context, int srctype, TDS_CHAR * src, TDS_UINT srclen, int desttype, TDS_CHAR * dest, SQLULEN destlen,
	     const struct _drecord *drec_ixd)
{
	TDS_INT nDestSybType;
	TDS_INT nRetVal = TDS_FAIL;

	CONV_RESULT ores;

	TDSDATEREC dr;
	DATE_STRUCT *dsp;
	TIME_STRUCT *tsp;
	TIMESTAMP_STRUCT *tssp;
	SQL_NUMERIC_STRUCT *num;

	int ret = TDS_CONVERT_FAIL;
	int i, cplen;

	tdsdump_log(TDS_DBG_FUNC, "odbc_tds2sql: src is %d dest = %d\n", srctype, desttype);

	assert(desttype != SQL_C_DEFAULT);

	nDestSybType = odbc_c_to_server_type(desttype);
	if (nDestSybType == TDS_FAIL)
		return TDS_CONVERT_NOAVAIL;

	/* special case for binary type */
	if (desttype == SQL_C_BINARY) {
		tdsdump_log(TDS_DBG_FUNC, "odbc_tds2sql: outputting binary data destlen = %lu \n", (unsigned long) destlen);

		if (is_numeric_type(srctype)) {
			desttype = SQL_C_NUMERIC;
			nDestSybType = SYBNUMERIC;
			/* prevent buffer overflow */
			if (destlen < sizeof(SQL_NUMERIC_STRUCT))
				return TDS_CONVERT_FAIL;
			ores.n.precision = ((TDS_NUMERIC *) src)->precision;
			ores.n.scale = ((TDS_NUMERIC *) src)->scale;
		} else {
			ret = srclen;
			if (destlen > 0) {
				cplen = (destlen > srclen) ? srclen : destlen;
				assert(cplen >= 0);
				/* do not NULL terminate binary buffer */
				memcpy(dest, src, cplen);
			} else {
				/* if destlen == 0 we return only length */
				if (destlen != 0)
					ret = TDS_CONVERT_FAIL;
			}
			return ret;
		}
	} else if (is_numeric_type(nDestSybType)) {
		/* TODO use descriptor information (APD) ?? However APD can contain SQL_C_DEFAULT... */
		if (drec_ixd)
			ores.n.precision = drec_ixd->sql_desc_precision;
		else
			ores.n.precision = 38;
		ores.n.scale = 0;
	}

	if (desttype == SQL_C_CHAR) {
		nDestSybType = TDS_CONVERT_CHAR;
		ores.cc.len = destlen >= 0 ? destlen : 0;
		ores.cc.c = dest;
	}

	if (desttype == SQL_C_CHAR && (srctype == SYBDATETIME || srctype == SYBDATETIME4)) {
		char buf[40];
		TDSDATEREC when;

		memset(&when, 0, sizeof(when));

		tds_datecrack(srctype, src, &when);
		tds_strftime(buf, sizeof(buf), srctype == SYBDATETIME ? "%Y-%m-%d %H:%M:%S.%z" : "%Y-%m-%d %H:%M:%S", &when);

		nRetVal = strlen(buf);
		memcpy(dest, buf, destlen >= 0 ? (destlen < nRetVal ? destlen : nRetVal) : 0);
	} else {
		nRetVal = tds_convert(context, srctype, src, srclen, nDestSybType, &ores);
	}
	if (nRetVal < 0)
		return nRetVal;

	switch (desttype) {

	case SQL_C_CHAR:
		tdsdump_log(TDS_DBG_FUNC, "odbc_tds2sql: outputting character data destlen = %lu \n", (unsigned long) destlen);

		ret = nRetVal;
		/* TODO handle not terminated configuration */
		if (destlen > 0) {
			cplen = (destlen - 1) > nRetVal ? nRetVal : (destlen - 1);
			assert(cplen >= 0);
			/*
			 * odbc always terminate but do not overwrite 
			 * destination buffer more than needed
			 */
			dest[cplen] = 0;
		} else {
			/* if destlen == 0 we return only length */
			if (destlen != 0)
				ret = TDS_CONVERT_FAIL;
		}
		break;

	case SQL_C_TYPE_DATE:
	case SQL_C_DATE:

		/* we've already converted the returned value to a SYBDATETIME */
		/* now decompose date into constituent parts...                */

		tds_datecrack(SYBDATETIME, &(ores.dt), &dr);

		dsp = (DATE_STRUCT *) dest;

		dsp->year = dr.year;
		dsp->month = dr.month + 1;
		dsp->day = dr.day;

		ret = sizeof(DATE_STRUCT);
		break;

	case SQL_C_TYPE_TIME:
	case SQL_C_TIME:

		/* we've already converted the returned value to a SYBDATETIME */
		/* now decompose date into constituent parts...                */

		tds_datecrack(SYBDATETIME, &(ores.dt), &dr);

		tsp = (TIME_STRUCT *) dest;

		tsp->hour = dr.hour;
		tsp->minute = dr.minute;
		tsp->second = dr.second;

		ret = sizeof(TIME_STRUCT);
		break;

	case SQL_C_TYPE_TIMESTAMP:
	case SQL_C_TIMESTAMP:

		/* we've already converted the returned value to a SYBDATETIME */
		/* now decompose date into constituent parts...                */

		tds_datecrack(SYBDATETIME, &(ores.dt), &dr);

		tssp = (TIMESTAMP_STRUCT *) dest;

		tssp->year = dr.year;
		tssp->month = dr.month + 1;
		tssp->day = dr.day;
		tssp->hour = dr.hour;
		tssp->minute = dr.minute;
		tssp->second = dr.second;
		tssp->fraction = dr.millisecond * 1000000u;

		ret = sizeof(TIMESTAMP_STRUCT);
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
		/* ODBC numeric is quite different from TDS one ... */
		num = (SQL_NUMERIC_STRUCT *) dest;
		num->precision = ores.n.precision;
		num->scale = ores.n.scale;
		num->sign = ores.n.array[0] ^ 1;
		/*
		 * TODO can be greater than SQL_MAX_NUMERIC_LEN ?? 
		 * seeing Sybase manual wire support bigger numeric but currently
		 * DBs so not support such precision
		 */
		i = ODBC_MIN(tds_numeric_bytes_per_prec[ores.n.precision] - 1, SQL_MAX_NUMERIC_LEN);
		memcpy(num->val, ores.n.array + 1, i);
		tds_swap_bytes(num->val, i);
		if (i < SQL_MAX_NUMERIC_LEN)
			memset(num->val + i, 0, SQL_MAX_NUMERIC_LEN - i);
		ret = sizeof(SQL_NUMERIC_STRUCT);
		break;

#ifdef SQL_C_GUID
	case SQL_C_GUID:
		memcpy(dest, &(ores.u), sizeof(TDS_UNIQUE));
		ret = sizeof(TDS_UNIQUE);
		break;
#endif

	case SQL_C_BINARY:
		/* type already handled */
		assert(desttype != SQL_C_BINARY);

	default:
		break;

	}

	return ret;
}
