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

#ifndef CONNECTPARAMS_H
#define CONNECTPARAMS_H

static char rcsid_connectparams_h[] = "$Id: connectparams.h,v 1.10 2003-09-03 19:04:14 freddy77 Exp $";
static void *no_unused_connectparams_h_warn[] = { rcsid_connectparams_h, no_unused_connectparams_h_warn };

/*****************************
 * odbc_parse_connect_string
 *
 * PURPOSE
 *
 *  Parses a connection string for SQLDriverConnect().
 *
 * ARGS
 *
 *  see ODBC documentation
 *                      
 * RETURNS
 *
 *  see ODBC documentation
 *
 * NOTE
 *
 *  - I doubt pszDataSourceName is useful here?
 *
 *****************************/
int odbc_parse_connect_string(const char *connect_string, const char *connect_string_end, TDSCONNECTINFO * connect_info);

int odbc_get_dsn_info(const char *DSN, TDSCONNECTINFO * connect_info);

#endif
