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
#include <tdsconvert.h>
#include "convert_tds2sql.h"
#include <time.h>
#include <assert.h>
#include <sqlext.h>

static char  software_version[]   = "$Id: convert_tds2sql.c,v 1.9 2002-09-14 13:49:03 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

/*
 * Pass this an SQL_* type and get a SYB* type which most closely corresponds
 * to the SQL_* type.
 *
 */
static int _odbc_get_server_type(int clt_type)
{
	switch (clt_type) {

	case SQL_C_CHAR:
		return SYBCHAR;

	case SQL_C_DATE:
	case SQL_C_TIME:
	case SQL_C_TIMESTAMP:
		return SYBDATETIME;

	case SQL_C_LONG:
	case SQL_C_ULONG:
	case SQL_C_SLONG:
		return SYBINT4;

	case SQL_C_SHORT:
	case SQL_C_USHORT:
	case SQL_C_SSHORT:
		return SYBINT2;

	case SQL_C_TINYINT:
	case SQL_C_UTINYINT:
	case SQL_C_STINYINT:
		return SYBINT1;

	case SQL_C_DOUBLE:
		return SYBFLT8;

	case SQL_C_FLOAT:
		return SYBREAL;

	case SQL_C_NUMERIC:
		return SYBNUMERIC;

	case SQL_C_BIT:
		return SYBBIT;

	}
	return TDS_FAIL;
}

TDS_INT 
convert_tds2sql(TDSCONTEXT *context, int srctype, TDS_CHAR *src, TDS_UINT srclen,
		int desttype, TDS_CHAR *dest, TDS_UINT destlen)
{
    TDS_INT nDestSybType;
    TDS_INT nRetVal;

    CONV_RESULT ores;

    TDSDATEREC        dr;
    DATE_STRUCT      *dsp;
    TIME_STRUCT      *tsp;
    TIMESTAMP_STRUCT *tssp;

    TDS_UINT         *uip;
    TDS_USMALLINT    *usip;

    int ret;
    int i,cplen;
        
        tdsdump_log(TDS_DBG_FUNC, "convert_tds2sql: src is %d dest = %d\n", srctype, desttype);

        nDestSybType = _odbc_get_server_type( desttype );

	if (is_numeric_type(nDestSybType)) {
		ores.n.precision = 18;
		ores.n.scale = 0;
	}
			

        if ( nDestSybType != TDS_FAIL )
        {
            nRetVal = tds_convert(context, srctype, src, srclen, nDestSybType, destlen, &ores);
        }
        
	/* FIXME if tds_convert fail we not return failure but continue... */
	    switch(desttype) {

           case SQL_C_CHAR:
             tdsdump_log(TDS_DBG_FUNC, "convert_tds2sql: outputting character data destlen = %d \n", destlen);

             if (destlen > 0) {
		cplen = (destlen-1) > nRetVal ? nRetVal : (destlen-1);
		assert(cplen >= 0);
		     /* odbc always terminate but do not overwrite 
		      * destination buffer more than needed */
                memcpy(dest, ores.c, cplen);
		dest[cplen] = 0;
                ret = nRetVal;
             }
             else {
		/* FIXME destlen == 0 is good... */
		if (destlen == 0)
			ret = nRetVal;
		else
			ret = TDS_FAIL;
             }

             free(ores.c);

             break;

		   case SQL_C_DATE:

             /* we've already converted the returned value to a SYBDATETIME */
             /* now decompose date into constituent parts...                */

             tds_datecrack(SYBDATETIME, &(ores.dt), &dr);

             dsp = (DATE_STRUCT *) dest;

             dsp->year  = dr.year;
             dsp->month = dr.month + 1;
             dsp->day   = dr.day;

             ret = sizeof(DATE_STRUCT);
             break;

		   case SQL_C_TIME:

             /* we've already converted the returned value to a SYBDATETIME */
             /* now decompose date into constituent parts...                */

             tds_datecrack(SYBDATETIME, &(ores.dt), &dr);

             tsp = (TIME_STRUCT *) dest;

             tsp->hour   = dr.hour;
             tsp->minute = dr.minute;
             tsp->second = dr.second;

             ret = sizeof(TIME_STRUCT);
             break;

		   case SQL_C_TIMESTAMP:

             /* we've already converted the returned value to a SYBDATETIME */
             /* now decompose date into constituent parts...                */

             tds_datecrack(SYBDATETIME, &(ores.dt), &dr);

             tssp = (TIMESTAMP_STRUCT *) dest;

             tssp->year     = dr.year;
             tssp->month    = dr.month + 1;
             tssp->day      = dr.day;
             tssp->hour     = dr.hour;
             tssp->minute   = dr.minute;
             tssp->second   = dr.second;
             tssp->fraction = dr.millisecond * 1000000;

             ret = sizeof(TIMESTAMP_STRUCT);
             break;

	       case SQL_C_LONG:
       	   case SQL_C_SLONG:
             memcpy(dest, &(ores.i), sizeof(TDS_INT));
             ret = sizeof(TDS_INT);
             break;

       	   case SQL_C_ULONG:
             uip  = (TDS_UINT *)dest;
             *uip = ores.i;
             ret  = sizeof(TDS_UINT);
             break;

	       case SQL_C_SHORT:
       	   case SQL_C_SSHORT:
             memcpy(dest, &(ores.si), sizeof(TDS_SMALLINT));
             ret = sizeof(TDS_SMALLINT);
             break;

       	   case SQL_C_USHORT:
             usip = (TDS_USMALLINT *)dest;
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
             ret  = sizeof(TDS_FLOAT);
             break;

	       case SQL_C_FLOAT:
             memcpy(dest, &(ores.r), sizeof(TDS_REAL));
             ret  = sizeof(TDS_REAL);
             break;

           case SQL_C_NUMERIC:
	     /* FIXME data should swapped, sign have same meaning ??? */
             memcpy(dest, &(ores.n), sizeof(TDS_NUMERIC));
             ret = sizeof(TDS_NUMERIC);
             break;
	    /* TODO GUID*/

           default:
             break;

        }

	    return ret;

}
