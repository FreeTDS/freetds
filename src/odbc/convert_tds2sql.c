/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2003-2004  Frediano Ziglio
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
#include "odbc_util.h"
#include "convert_tds2sql.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: convert_tds2sql.c,v 1.38 2004-03-08 12:51:57 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

TDS_INT
convert_tds2sql(TDSCONTEXT * context, int srctype, TDS_CHAR * src, TDS_UINT srclen, int desttype, TDS_CHAR * dest, TDS_UINT destlen)
{
	TDS_INT nDestSybType;
	TDS_INT nRetVal = TDS_FAIL;

	CONV_RESULT ores;

	TDSDATEREC dr;
	DATE_STRUCT *dsp;
	TIME_STRUCT *tsp;
	TIMESTAMP_STRUCT *tssp;
	SQL_NUMERIC_STRUCT *num;

	TDS_UINT *uip;
	TDS_USMALLINT *usip;

	int ret = TDS_FAIL;
	int i, cplen;

	tdsdump_log(TDS_DBG_FUNC, "convert_tds2sql: src is %d dest = %d\n", srctype, desttype);

	assert(desttype != SQL_C_DEFAULT);

	/* special case for binary type */
	if (desttype == SQL_C_BINARY) {
		tdsdump_log(TDS_DBG_FUNC, "convert_tds2sql: outputting binary data destlen = %d \n", destlen);

		if (is_numeric_type(srctype)) {
			desttype = SQL_C_NUMERIC;
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
					ret = TDS_FAIL;
			}
			return ret;
		}
	}

	nDestSybType = odbc_c_to_server_type(desttype);
	if (nDestSybType == TDS_FAIL)
		return TDS_CONVERT_NOAVAIL;

	if (is_numeric_type(nDestSybType)) {
		/* TODO use descriptor information (APD) ?? However APD can contain SQL_C_DEFAULT... */
		ores.n.precision = 38;
		ores.n.scale = 0;
	}


	nRetVal = tds_convert(context, srctype, src, srclen, nDestSybType, &ores);
	if (nRetVal < 0)
		return nRetVal;

	switch (desttype) {

	case SQL_C_CHAR:
		tdsdump_log(TDS_DBG_FUNC, "convert_tds2sql: outputting character data destlen = %d \n", destlen);

		ret = nRetVal;
		/* TODO handle not terminated configuration */
		if (destlen > 0) {
			cplen = (destlen - 1) > nRetVal ? nRetVal : (destlen - 1);
			assert(cplen >= 0);
			/* odbc always terminate but do not overwrite 
			 * destination buffer more than needed */
			memcpy(dest, ores.c, cplen);
			dest[cplen] = 0;
		} else {
			/* if destlen == 0 we return only length */
			if (destlen != 0)
				ret = TDS_FAIL;
		}

		free(ores.c);
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
		memcpy(dest, &(ores.bi), sizeof(TDS_INT8));
		ret = sizeof(TDS_INT8);
		break;
#endif

	case SQL_C_LONG:
	case SQL_C_SLONG:
		memcpy(dest, &(ores.i), sizeof(TDS_INT));
		ret = sizeof(TDS_INT);
		break;

	case SQL_C_ULONG:
		uip = (TDS_UINT *) dest;
		*uip = ores.i;
		ret = sizeof(TDS_UINT);
		break;

	case SQL_C_SHORT:
	case SQL_C_SSHORT:
		memcpy(dest, &(ores.si), sizeof(TDS_SMALLINT));
		ret = sizeof(TDS_SMALLINT);
		break;

	case SQL_C_USHORT:
		usip = (TDS_USMALLINT *) dest;
		*usip = ores.si;
		ret = sizeof(TDS_USMALLINT);
		break;

	case SQL_C_TINYINT:
	case SQL_C_STINYINT:
	case SQL_C_UTINYINT:
	case SQL_C_BIT:
		memcpy(dest, &(ores.ti), sizeof(TDS_TINYINT));
		ret = sizeof(TDS_TINYINT);
		break;

	case SQL_C_DOUBLE:
		memcpy(dest, &(ores.f), sizeof(TDS_FLOAT));
		ret = sizeof(TDS_FLOAT);
		break;

	case SQL_C_FLOAT:
		memcpy(dest, &(ores.r), sizeof(TDS_REAL));
		ret = sizeof(TDS_REAL);
		break;

	case SQL_C_NUMERIC:
		/* ODBC numeric is quite different from TDS one ... */
		num = (SQL_NUMERIC_STRUCT *) dest;
		num->precision = ores.n.precision;
		num->scale = ores.n.scale;
		num->sign = ores.n.array[0] ^ 1;
		/* FIXME can i be greater than SQL_MAX_NUMERIC_LEN ?? */
		i = tds_numeric_bytes_per_prec[ores.n.precision] - 1;
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
