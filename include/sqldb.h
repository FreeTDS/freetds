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

#ifndef SQLDB_h
#define SQLDB_h

#include <sybdb.h>

/* 
 * TODO: add other Microsoft differences here e.g. SQLMONEY for SYBMONEY
 */
#define dbfreelogin(x) dbloginfree((x))

static const char rcsid_sqldb_h[] = "$Id: sqldb.h,v 1.4 2007-10-24 21:57:32 jklowden Exp $";
static const void *const no_unused_sqldb_h_warn[] = { rcsid_sqldb_h, no_unused_sqldb_h_warn };


#endif
