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

#ifdef UNIXODBC
#ifndef HAVEODBCINST
#define HAVEODBCINST
#endif
#endif

static char rcsid_connectparams_h[] = "$Id: connectparams.h,v 1.7.2.1 2003-08-18 16:08:24 freddy77 Exp $";
static void *no_unused_connectparams_h_warn[] = { rcsid_connectparams_h, no_unused_connectparams_h_warn };

/*****************************
 * tdoParseConnectString
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
int tdoParseConnectString(const char *connect_string, const char *connect_string_end, TDSCONNECTINFO * connect_info);

int odbc_get_dsn_info(const char *DSN, TDSCONNECTINFO * connect_info);

#ifndef HAVEODBCINST
/*****************************
 * SQLGetPrivateProfileString
 *
 * PURPOSE
 *
 *  This is an implementation of a common MS API call. This implementation 
 *  should only be used if the ODBC sub-system/SDK does not have it.
 *  For example; unixODBC has its own so those using unixODBC should NOT be
 *  using this implementation because unixODBC;
 *  - provides caching of ODBC config data 
 *  - provides consistent interpretation of ODBC config data (i.e, location)
 *
 * ARGS
 *
 *  see ODBC documentation
 *                      
 * RETURNS
 *
 *  see ODBC documentation
 *
 * NOTES:
 *
 *  - the spec is not entirely implemented... consider this a lite version
 *  - rules for determining the location of ODBC config may be different then what you 
 *    expect see tdoGetIniFileName().
 *
 *****************************/
int SQLGetPrivateProfileString(LPCSTR pszSection, LPCSTR pszEntry, LPCSTR pszDefault, LPSTR pRetBuffer, int nRetBuffer,
			       LPCSTR pszFileName);
#endif

#endif
