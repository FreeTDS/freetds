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

static char  software_version[]   = "$Id: dbopen.c,v 1.4 2002-09-27 03:09:52 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};
#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "sybdb.h"

#ifdef dbopen
#undef dbopen
#endif

DBPROCESS *
dbopen(LOGINREC *login, char *server)
{
	return tdsdbopen(login, server);
}
