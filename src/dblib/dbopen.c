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

#include <freetds/tds.h>
#include <sybdb.h>
#include <dblib.h>

#ifdef dbopen
#undef dbopen
#endif

/**
 * Normally not used. 
 * The function is linked in only if the --enable-sybase-compat configure option is used.  
 * Cf. sybdb.h dbopen() macros, and dbdatecrack(). 
 */
DBPROCESS *
dbopen(LOGINREC * login, const char *server)
{
	return tdsdbopen(login, server, dblib_msdblib);
}
