/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2003  Steve Murphree
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

#include "tds.h"
#include "tdsodbc.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static void desc_free_record(struct _drecord *drec);

TDS_DESC *
desc_alloc(SQLHDESC parent, int desc_type, int alloc_type)
{
	TDS_DESC *desc;

	desc = (TDS_DESC *) malloc(sizeof(TDS_DESC));
	if (!desc)
		return NULL;
	memset(desc, 0, sizeof(TDS_DESC));

	/* set defualt header values */
	desc->htype = SQL_HANDLE_DESC;
	desc->type = desc_type;
	desc->parent = parent;
	desc->header.sql_desc_alloc_type = alloc_type;
	desc->header.sql_desc_count = 0;
	desc->records = NULL;

	switch (desc_type) {
	case DESC_IRD:
	case DESC_IPD:
		break;
	case DESC_ARD:
	case DESC_APD:
		desc->header.sql_desc_bind_type = SQL_BIND_BY_COLUMN;
		desc->header.sql_desc_array_size = 1;
		break;
	default:
		free(desc);
		return NULL;
	}
	return desc;
}

SQLRETURN
desc_alloc_records(TDS_DESC * desc, unsigned count)
{
	struct _drecord *drec, *drecs;
	int i;

	/* shrink records */
	if (desc->header.sql_desc_count >= count) {
		for (i = count; i < desc->header.sql_desc_count; ++i)
			desc_free_record(&desc->records[i]);
		desc->header.sql_desc_count = count;
		return SQL_SUCCESS;
	}

	if (desc->records)
		drecs = (struct _drecord *) realloc(desc->records, sizeof(struct _drecord) * (count + 0));
	else
		drecs = (struct _drecord *) malloc(sizeof(struct _drecord) * (count + 0));
	if (!drecs)
		return SQL_ERROR;
	desc->records = drecs;
	memset(desc->records + desc->header.sql_desc_count, 0, sizeof(struct _drecord) * (desc->header.sql_desc_count - count));

	for (i = desc->header.sql_desc_count; i < count; ++i) {
		drec = &desc->records[i];
		switch (desc->type) {
		case DESC_IRD:
		case DESC_IPD:
			drec->sql_desc_parameter_type = SQL_PARAM_INPUT;
			break;
		case DESC_ARD:
		case DESC_APD:
			drec->sql_desc_concise_type = SQL_C_DEFAULT;
			drec->sql_desc_type = SQL_C_DEFAULT;
			break;
		}
	}
	desc->header.sql_desc_count = count;
	return SQL_SUCCESS;
}

#define IF_FREE(x) if (x) free(x)

static void
desc_free_record(struct _drecord *drec)
{
	IF_FREE(drec->sql_desc_base_column_name);
	IF_FREE(drec->sql_desc_base_table_name);
	IF_FREE(drec->sql_desc_catalog_name);
	IF_FREE(drec->sql_desc_label);
	IF_FREE(drec->sql_desc_literal_prefix);
	IF_FREE(drec->sql_desc_literal_suffix);
	IF_FREE(drec->sql_desc_local_type_name);
	IF_FREE(drec->sql_desc_name);
	IF_FREE(drec->sql_desc_schema_name);
	IF_FREE(drec->sql_desc_table_name);
	IF_FREE(drec->sql_desc_type_name);
}

SQLRETURN
desc_free_records(TDS_DESC * desc)
{
	int i;

	if (desc->records) {
		for (i = 0; i < desc->header.sql_desc_count; i++)
			desc_free_record(&desc->records[i]);
		free(desc->records);
		desc->records = NULL;
	}

	desc->header.sql_desc_count = 0;
	return SQL_SUCCESS;
}

SQLRETURN
desc_free(TDS_DESC * desc)
{
	if (desc) {
		desc_free_records(desc);
		free(desc);
	}
	return SQL_SUCCESS;
}
