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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "tds.h"
#include "sybfront.h"
#include "sybdb.h"
#include "dblib.h"

static char  software_version[]   = "$Id: xact.c,v 1.6 2002-11-01 20:55:48 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


void build_xact_string(char *xact_name, char *service_name, DBINT commid, char *result)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED build_xact_string()\n");
}
RETCODE remove_xact(DBPROCESS *connect, DBINT commid, int n)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED remove_xact()\n");
	return SUCCEED;
}
RETCODE abort_xact(DBPROCESS *connect, DBINT commid)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED abort_xact()\n");
	return SUCCEED;
}
void close_commit(DBPROCESS *connect)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED close_commit()\n");
}
RETCODE commit_xact(DBPROCESS *connect, DBINT commid)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED commit_xact()\n");
	return SUCCEED;
}
DBPROCESS *open_commit(LOGINREC *login, char *servername)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED open_commit()\n");
	return NULL;
}
RETCODE scan_xact(DBPROCESS *connect, DBINT commid)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED scan_xact()\n");
	return SUCCEED;
}
DBINT start_xact(DBPROCESS *connect, char *application_name, char *xact_name, int site_count)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED start_xact()\n");
	return 0;
}
DBINT stat_xact(DBPROCESS *connect, DBINT commid)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED stat_xact()\n");
	return 0;
}
