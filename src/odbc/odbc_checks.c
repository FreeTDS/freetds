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
#include "odbc_checks.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: odbc_checks.c,v 1.3 2003-08-30 17:10:36 freddy77 Exp $";
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
}

static void
odbc_check_drecord(struct _drecord *drec)
{
	const int invalid_desc_type = 0;
	const int invalid_datetime_interval_code = 0;
	int concise_type = 0;

	assert(drec->sql_desc_concise_type != SQL_INTERVAL && drec->sql_desc_concise_type != SQL_DATETIME);

	switch (drec->sql_desc_type) {
	case SQL_BIT:
	case SQL_SMALLINT:
	case SQL_TINYINT:
	case SQL_INTEGER:
	case SQL_BIGINT:

	case SQL_GUID:

	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_LONGVARBINARY:

	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:

	case SQL_DECIMAL:
	case SQL_NUMERIC:

	case SQL_FLOAT:
	case SQL_REAL:
	case SQL_DOUBLE:
		assert(drec->sql_desc_type == drec->sql_desc_concise_type);
		assert(drec->sql_desc_datetime_interval_code == 0);
		break;

	case SQL_DATETIME:
		switch (drec->sql_desc_datetime_interval_code) {
		case SQL_CODE_DATE:
			concise_type = SQL_TYPE_DATE;
			break;
		case SQL_CODE_TIME:
			concise_type = SQL_TYPE_TIME;
			break;
		case SQL_CODE_TIMESTAMP:
			concise_type = SQL_TYPE_TIMESTAMP;
			break;
		default:
			assert(invalid_datetime_interval_code);
			break;
		}
		assert(drec->sql_desc_concise_type == concise_type);
		break;

	case SQL_INTERVAL:
		switch (drec->sql_desc_datetime_interval_code) {
		case SQL_CODE_DAY:
			concise_type = SQL_INTERVAL_DAY;
			break;
		case SQL_CODE_DAY_TO_HOUR:
			concise_type = SQL_INTERVAL_DAY_TO_HOUR;
			break;
		case SQL_CODE_DAY_TO_MINUTE:
			concise_type = SQL_INTERVAL_DAY_TO_MINUTE;
			break;
		case SQL_CODE_DAY_TO_SECOND:
			concise_type = SQL_INTERVAL_DAY_TO_SECOND;
			break;
		case SQL_CODE_HOUR:
			concise_type = SQL_INTERVAL_HOUR;
			break;
		case SQL_CODE_HOUR_TO_MINUTE:
			concise_type = SQL_INTERVAL_HOUR_TO_MINUTE;
			break;
		case SQL_CODE_HOUR_TO_SECOND:
			concise_type = SQL_INTERVAL_HOUR_TO_SECOND;
			break;
		case SQL_CODE_MINUTE:
			concise_type = SQL_INTERVAL_MINUTE;
			break;
		case SQL_CODE_MINUTE_TO_SECOND:
			concise_type = SQL_INTERVAL_MINUTE_TO_SECOND;
			break;
		case SQL_CODE_MONTH:
			concise_type = SQL_INTERVAL_MONTH;
			break;
		case SQL_CODE_SECOND:
			concise_type = SQL_INTERVAL_SECOND;
			break;
		case SQL_CODE_YEAR:
			concise_type = SQL_INTERVAL_YEAR;
			break;
		case SQL_CODE_YEAR_TO_MONTH:
			concise_type = SQL_INTERVAL_YEAR_TO_MONTH;
			break;
		default:
			assert(invalid_datetime_interval_code);
			break;
		}
		assert(drec->sql_desc_concise_type == concise_type);
		break;

	default:
		assert(invalid_desc_type);
	}
}

void
odbc_check_desc_extra(TDS_DESC * desc)
{
	int i;

	assert(desc && desc->htype == SQL_HANDLE_DESC);
	assert(desc->header.sql_desc_alloc_type == SQL_DESC_ALLOC_AUTO || desc->header.sql_desc_alloc_type == SQL_DESC_ALLOC_USER);
	for (i = 0; i < desc->header.sql_desc_count; ++i) {
		odbc_check_drecord(&desc->records[i]);
	}
}

#endif /* ENABLE_EXTRA_CHECKS */
