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

#include <config.h>
#include <tdsutil.h>
#include <tds.h>
#include "convert_tds2sql.h"
#include <time.h>
#include <assert.h>
#include <sqlext.h>

static char  software_version[]   = "$Id: convert_tds2sql.c,v 1.1 2002-05-27 20:12:43 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

static int _odbc_get_server_type(int clt_type)
{
	switch (clt_type) {
	case SQL_CHAR:
	case SQL_VARCHAR:
		return SYBCHAR;
	case SQL_BIT:
		return SYBBIT;
	case SQL_TINYINT:
		return SYBINT1;
	case SQL_SMALLINT:
		return SYBINT2;
	case SQL_INTEGER:
		return SYBINT4;
	case SQL_DOUBLE:
		return SYBFLT8;
	case SQL_DECIMAL:
		return SYBDECIMAL;
	case SQL_NUMERIC:
		return SYBNUMERIC;
	case SQL_FLOAT:
		return SYBREAL;
	case SQL_LONGVARCHAR:
		return SYBTEXT;
	case SQL_BINARY:
		return SYBBINARY;
	}
	return TDS_FAIL;
}


static TDS_INT 
convert_datetime2sql(TDSLOCINFO *locale,int srctype,TDS_CHAR *src,
	int desttype,TDS_CHAR *dest,TDS_INT destlen)
{
	time_t           tmp_secs_from_epoch;
	TDS_DATETIME     *src_d  = (TDS_DATETIME*)src;
	TDS_DATETIME4    *src_d4 = (TDS_DATETIME4*)src;

	switch(desttype) {
	case SQL_TIMESTAMP:
	/* FIX ME -- This fails for dates before 1902 or after 2038 */
		{
			TIMESTAMP_STRUCT *dest_d = (TIMESTAMP_STRUCT*)dest;
			struct tm t;

			if (!src || !dest || destlen<sizeof(TIMESTAMP_STRUCT))
				return TDS_FAIL;

			if (SYBDATETIME==srctype)
			    tmp_secs_from_epoch = ((src_d->dtdays - 25567)*24*60*60) + (src_d->dttime/300);
			else
			    tmp_secs_from_epoch = ((src_d4->days - 25567)*24*60*60) + (src_d4->minutes*60);
			// This function is not thread safe !!!
			gmtime_r(&tmp_secs_from_epoch, &t);

			dest_d->year     = t.tm_year + 1900;
			dest_d->month    = t.tm_mon  + 1;
			dest_d->day      = t.tm_mday;
			dest_d->hour     = t.tm_hour;
			dest_d->minute   = t.tm_min;
			dest_d->second   = t.tm_sec;
			dest_d->fraction = 0;

			return sizeof(TIMESTAMP_STRUCT);
		}
		break;
	}
	return TDS_FAIL;
}


TDS_INT 
convert_tds2sql(TDSLOCINFO *locale, int srctype, TDS_CHAR *src, TDS_UINT srclen,
		int desttype, TDS_CHAR *dest, TDS_UINT destlen)
{
	if (TDS_FAIL!=_odbc_get_server_type(desttype))
		return tds_convert(locale, 
		srctype,
		src,
		srclen, 
		_odbc_get_server_type(desttype), 
		dest, 
		destlen);


	switch(srctype) {
//		case SYBCHAR:
//		case SYBVARCHAR:
//		case SYBNVARCHAR:
//			break;
//		case SYBMONEY4:
//			break;
//		case SYBMONEY:
//			break;
//		case SYBNUMERIC:
//		case SYBDECIMAL:
//			break;
//		case SYBBIT:
//		case SYBBITN:
//			break;
//		case SYBINT1:
//			break;
//		case SYBINT2:
//			break;
//		case SYBINT4:
//			break;
//		case SYBREAL:
//			break;
//		case SYBFLT8:
//			break;
		case SYBDATETIME:
		case SYBDATETIME4:
			convert_datetime2sql(locale,srctype,src,desttype,dest,destlen);
			break;
//		case SYBVARBINARY:
//			break;
//		case SYBIMAGE:
//		case SYBBINARY:
//			break;
//		case SYBTEXT:
//			break;
//		case SYBNTEXT:
//			break;
		default:
			fprintf(stderr,"convert_tds2sql(): Attempting to convert unknown "
				       "source type %d (size %d) into %d (size %d) \n",
				       srctype,srclen,desttype,destlen);

	}
	return TDS_FAIL;
}
