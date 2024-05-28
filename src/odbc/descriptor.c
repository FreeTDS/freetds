/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2003  Steve Murphree
 * Copyright (C) 2004, 2005  Ziglio Frediano
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

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/odbc.h>
#include <freetds/utils/string.h>
#include <odbcss.h>

static void desc_free_record(struct _drecord *drec);

TDS_DESC *
desc_alloc(SQLHANDLE parent, int desc_type, SQLSMALLINT alloc_type)
{
	TDS_DESC *desc;

	desc = tds_new0(TDS_DESC, 1);
	if (!desc || tds_mutex_init(&desc->mtx)) {
		free(desc);
		return NULL;
	}

	/* set default header values */
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
	CHECK_DESC_EXTRA(desc);
	return desc;
}

#define SQL_DESC_STRINGS \
	STR_OP(sql_desc_base_column_name); \
	STR_OP(sql_desc_base_table_name); \
	STR_OP(sql_desc_catalog_name); \
	STR_OP(sql_desc_label); \
	STR_OP(sql_desc_local_type_name); \
	STR_OP(sql_desc_name); \
	STR_OP(sql_desc_schema_name); \
	STR_OP(sql_desc_table_name)

SQLRETURN
desc_alloc_records(TDS_DESC * desc, SQLSMALLINT count)
{
	struct _drecord *drec;
	int i;

	/* shrink records */
	if (desc->header.sql_desc_count >= count) {
		for (i = count; i < desc->header.sql_desc_count; ++i)
			desc_free_record(&desc->records[i]);
		desc->header.sql_desc_count = count;
		return SQL_SUCCESS;
	}

	if (!TDS_RESIZE(desc->records, count))
		return SQL_ERROR;
	memset(desc->records + desc->header.sql_desc_count, 0, sizeof(struct _drecord) * (count - desc->header.sql_desc_count));

	for (i = desc->header.sql_desc_count; i < count; ++i) {
		drec = &desc->records[i];

#define STR_OP(name) tds_dstr_init(&drec->name)
		SQL_DESC_STRINGS;
#undef STR_OP

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

static void
desc_free_record(struct _drecord *drec)
{
#define STR_OP(name) tds_dstr_free(&drec->name)
	SQL_DESC_STRINGS;
#undef STR_OP
	if (drec->sql_desc_concise_type == SQL_SS_TABLE)
		tvp_free((SQLTVP *) drec->sql_desc_data_ptr);
}

SQLRETURN
desc_free_records(TDS_DESC * desc)
{
	int i;

	if (desc->records) {
		for (i = 0; i < desc->header.sql_desc_count; i++)
			desc_free_record(&desc->records[i]);
		TDS_ZERO_FREE(desc->records);
	}

	desc->header.sql_desc_count = 0;
	return SQL_SUCCESS;
}

SQLRETURN
desc_copy(TDS_DESC * dest, TDS_DESC * src)
{
	int i;
	TDS_DESC tmp = *dest;

	/* copy header */
	tmp.header = src->header;

	/* sql_desc_alloc_type should remain unchanged */
	tmp.header.sql_desc_alloc_type = dest->header.sql_desc_alloc_type;

	/* set no records */
	tmp.header.sql_desc_count = 0;
	tmp.records = NULL;

	tmp.errs.num_errors = 0;
	tmp.errs.errs = NULL;

	if (desc_alloc_records(&tmp, src->header.sql_desc_count) != SQL_SUCCESS)
		return SQL_ERROR;

	for (i = 0; i < src->header.sql_desc_count; ++i) {
		struct _drecord *src_rec = &src->records[i];
		struct _drecord *dest_rec = &tmp.records[i];

		/* copy all integers at once ! */
		memcpy(dest_rec, src_rec, sizeof(struct _drecord));

		/* reinitialize string, avoid doubling pointers */
#define STR_OP(name) tds_dstr_init(&dest_rec->name)
		SQL_DESC_STRINGS;
#undef STR_OP

		/* copy strings */
#define STR_OP(name) if (!tds_dstr_dup(&dest_rec->name, &src_rec->name)) goto Cleanup
		SQL_DESC_STRINGS;
#undef STR_OP
	}

	/* success, copy back to our descriptor */
	desc_free_records(dest);
	odbc_errs_reset(&dest->errs);
	*dest = tmp;
	return SQL_SUCCESS;

Cleanup:
	desc_free_records(&tmp);
	odbc_errs_reset(&tmp.errs);
	return SQL_ERROR;
}

SQLRETURN
desc_free(TDS_DESC * desc)
{
	if (desc) {
		desc_free_records(desc);
		odbc_errs_reset(&desc->errs);
		tds_mutex_free(&desc->mtx);
		free(desc);
	}
	return SQL_SUCCESS;
}

TDS_DBC *
desc_get_dbc(TDS_DESC *desc)
{
	if (IS_HSTMT(desc->parent))
		return ((TDS_STMT *) desc->parent)->dbc;

	return (TDS_DBC *) desc->parent;
}

SQLTVP *
tvp_alloc(TDS_STMT *stmt)
{
	SQLTVP *tvp = tds_new0(SQLTVP, 1);
	tds_dstr_init(&tvp->type_name);
	tvp->ipd = desc_alloc(stmt, DESC_IPD, SQL_DESC_ALLOC_AUTO);
	tvp->apd = desc_alloc(stmt, DESC_APD, SQL_DESC_ALLOC_AUTO);
	if (!tvp->ipd || !tvp->apd) {
		tvp_free(tvp);
		return NULL;
	}
	tvp->ipd->focus = -1;
	tvp->apd->focus = -1;
	return tvp;
}

void
tvp_free(SQLTVP *tvp)
{
	if (!tvp)
		return;

	desc_free(tvp->ipd);
	desc_free(tvp->apd);
	tds_dstr_free(&tvp->type_name);
	free(tvp);
}
