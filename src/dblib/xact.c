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

#include "tdsutil.h"
#include "tds.h"
#include "sybfront.h"
#include "sybdb.h"
#include "dblib.h"
#include <unistd.h>

static char  software_version[]   = "$Id: xact.c,v 1.1 2001-10-12 23:29:10 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


void build_xact_string(char *xact_name, char *service_name, DBINT commid, char *result)
{
}
RETCODE remove_xact(DBPROCESS *connect, DBINT commid, int n)
{
	return SUCCEED;
}
RETCODE abort_xact(DBPROCESS *connect, DBINT commid)
{
	return SUCCEED;
}
void close_commit(DBPROCESS *connect)
{
}
RETCODE commit_xact(DBPROCESS *connect, DBINT commid)
{
	return SUCCEED;
}
DBPROCESS *open_commit(LOGINREC *login, char *servername)
{
	return NULL;
}
RETCODE scan_xact(DBPROCESS *connect, DBINT commid)
{
	return SUCCEED;
}
DBINT start_xact(DBPROCESS *connect, char *application_name, char *xact_name, int site_count)
{
	return 0;
}
DBINT stat_xact(DBPROCESS *connect, DBINT commid)
{
	return 0;
}
