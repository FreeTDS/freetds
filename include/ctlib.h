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

#ifndef _ctlib_h_
#define _ctlib_h_
/*
** Internal (not part of the exposed API) prototypes and such.
*/
#ifdef __cplusplus
extern "C" {
#endif

static char  rcsid_ctlib_h [ ] =
         "$Id: ctlib.h,v 1.1 2001-10-12 23:28:56 brianb Exp $";
static void *no_unused_ctlib_h_warn[]={rcsid_ctlib_h, no_unused_ctlib_h_warn};

#include <tds.h>

#define DBLIB_INFO_MSG_TYPE 0
#define DBLIB_ERROR_MSG_TYPE 1

/*
** internal typedefs
*/
typedef struct ctcolinfo
{
	TDS_SMALLINT *indicator;
} CT_COLINFO;

/*
** internal prototypes
*/
int ctlib_handle_info_message(void *aStruct);
int ctlib_handle_err_message(void *aStruct);
int _ct_get_server_type(int datatype);

#ifdef __cplusplus
}
#endif

#endif
