/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2003 Frediano Ziglio
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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <assert.h>

#include "tds.h"
#include "tdsodbc.h"
#include "tdsstring.h"
#include "odbc_util.h"
#include "odbc_checks.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: odbc_checks.c,v 1.7 2003-11-04 19:01:47 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#if ENABLE_EXTRA_CHECKS

void
odbc_check_stmt_extra(TDS_STMT * stmt)
{
	assert(stmt && stmt->htype == SQL_HANDLE_STMT);
	/* TODO deep check on connection */
	assert(stmt->hdbc);
	odbc_check_desc_extra(stmt->ard);
	odbc_check_desc_extra(stmt->ird);
	odbc_check_desc_extra(stmt->apd);
	odbc_check_desc_extra(stmt->ipd);
	assert(!stmt->prepared_query_is_func || stmt->prepared_query_is_rpc);
	assert(stmt->param_num <= stmt->param_count + 1);
}

static void
odbc_check_drecord(TDS_DESC * desc, struct _drecord *drec)
{
	assert(drec->sql_desc_concise_type != SQL_INTERVAL && drec->sql_desc_concise_type != SQL_DATETIME);

	if (desc->type == DESC_IPD || desc->type == DESC_IRD) {
		assert(odbc_get_concise_sql_type(drec->sql_desc_type, drec->sql_desc_datetime_interval_code) ==
		       drec->sql_desc_concise_type && drec->sql_desc_concise_type != 0);
	} else {
		assert(odbc_get_concise_c_type(drec->sql_desc_type, drec->sql_desc_datetime_interval_code) ==
		       drec->sql_desc_concise_type && drec->sql_desc_concise_type != 0);
	}
}

void
odbc_check_desc_extra(TDS_DESC * desc)
{
	int i;

	assert(desc && desc->htype == SQL_HANDLE_DESC);
	assert(desc->header.sql_desc_alloc_type == SQL_DESC_ALLOC_AUTO || desc->header.sql_desc_alloc_type == SQL_DESC_ALLOC_USER);
	assert((desc->type != DESC_IPD && desc->type != DESC_IRD) || desc->header.sql_desc_alloc_type == SQL_DESC_ALLOC_AUTO);
	for (i = 0; i < desc->header.sql_desc_count; ++i) {
		odbc_check_drecord(desc, &desc->records[i]);
	}
}

void
odbc_check_struct_extra(void *p)
{
	const int invalid_htype = 0;

	switch (((struct _hchk *) p)->htype) {
	case SQL_HANDLE_ENV:
		break;
	case SQL_HANDLE_DBC:
		break;
	case SQL_HANDLE_STMT:
		odbc_check_stmt_extra((TDS_STMT *) p);
		break;
	case SQL_HANDLE_DESC:
		odbc_check_desc_extra((TDS_DESC *) p);
		break;
	default:
		assert(invalid_htype);
	}
}

#endif /* ENABLE_EXTRA_CHECKS */
