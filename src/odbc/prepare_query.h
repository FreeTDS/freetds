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

#ifndef PREPARE_QUERY_h
#define PREPARE_QUERY_h

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if defined(UNIXODBC) || defined(TDS_NO_DM)
#include <sql.h>
#include <sqlext.h>
#else /* iODBC */
#include "isql.h"
#include "isqlext.h"
#endif


static char rcsid_prepare_query_h[] = "$Id: prepare_query.h,v 1.6 2003-11-02 09:59:33 freddy77 Exp $";
static void *no_unused_prepare_query_h_warn[] = { rcsid_prepare_query_h, no_unused_prepare_query_h_warn };


SQLRETURN prepare_call(struct _hstmt *stmt);
SQLRETURN native_sql(char *s);
int start_parse_prepared_query(struct _hstmt *stmt);
int continue_parse_prepared_query(struct _hstmt *stmt, SQLPOINTER DataPtr, SQLINTEGER StrLen_or_Ind);

#endif
