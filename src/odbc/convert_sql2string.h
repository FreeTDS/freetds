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

#ifndef CONVERT_SQL2STRING_h
#define CONVERT_SQL2STRING_h

static char  rcsid_convert_sql2string_h [ ] =
         "$Id: convert_sql2string.h,v 1.2 2002-07-15 03:29:58 brianb Exp $";
static void *no_unused_convert_sql2string_h_warn[]={rcsid_convert_sql2string_h, 
                                         no_unused_convert_sql2string_h_warn};



TDS_INT convert_sql2string(TDSCONTEXT *context, int srctype, TDS_CHAR *src, 
		TDS_INT srclen, TDS_CHAR *dest, TDS_INT destlen,
		int param_lenbind);

#endif
