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

#ifndef TDSCONVERT_h
#define TDSCONVERT_h

static char  rcsid_tdsconvert_h [ ] =
         "$Id: tdsconvert.h,v 1.6 2002-07-11 05:55:44 jklowden Exp $";
static void *no_unused_tdsconvert_h_warn[]={rcsid_tdsconvert_h, 
                                         no_unused_tdsconvert_h_warn};

struct tds_tm;
static size_t tds_strftime(char *buf, size_t maxsize, const char *format, 
				const struct tds_tm *timeptr);


extern TDS_INT _convert_money(int srctype,unsigned char *src,
                            int desttype,unsigned char *dest,TDS_INT destlen);
extern TDS_INT _convert_bit(int srctype,unsigned char *src,
	int desttype,unsigned char *dest,TDS_INT destlen);
extern TDS_INT _convert_int1(int srctype,unsigned char *src,
	int desttype,unsigned char *dest,TDS_INT destlen);
extern TDS_INT _convert_int2(int srctype,unsigned char *src,
	int desttype,unsigned char *dest,TDS_INT destlen);
extern TDS_INT _convert_int4(int srctype,unsigned char *src,
	int desttype,unsigned char *dest,TDS_INT destlen);
extern TDS_INT _convert_flt8(int srctype,unsigned char *src,int desttype,unsigned char *dest,TDS_INT destlen);
extern TDS_INT _convert_datetime(int srctype,unsigned char *src,int desttype,unsigned char *dest,TDS_INT destlen);
extern int _get_conversion_type(int srctype, int colsize);

extern unsigned char tds_willconvert(int srctype, int desttype);

TDS_INT tds_convert(TDSLOCINFO *locale, int srctype, TDS_CHAR *src, 
		TDS_UINT srclen, int desttype, TDS_CHAR *dest, TDS_UINT destlen);

struct  tds_time {
int tm_year;
int tm_mon;
int tm_mday;
int tm_hour;
int tm_min;
int tm_sec;
int tm_ms;
};

struct tds_tm {
	struct tm tm;
	int milliseconds;
};
#endif

