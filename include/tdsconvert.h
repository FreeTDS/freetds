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
         "$Id: tdsconvert.h,v 1.3 2002-02-17 20:23:37 brianb Exp $";
static void *no_unused_tdsconvert_h_warn[]={rcsid_tdsconvert_h, 
                                         no_unused_tdsconvert_h_warn};



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
TDS_INT tds_convert(TDSLOCINFO *locale, int srctype, TDS_CHAR *src, 
		TDS_UINT srclen, int desttype, TDS_CHAR *dest, TDS_UINT destlen);

#endif
