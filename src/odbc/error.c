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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#include "tdsodbc.h"
#include "odbc_util.h"
#include "convert_tds2sql.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: error.c,v 1.1 2003-01-03 14:37:22 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define ODBCERR(s2,s3,msg) { msg, s2, s3 }
static const struct _sql_error_struct odbc_errs[] = {
	ODBCERR("S1000", "HY000", "General driver error"),
	ODBCERR("S1C00", "HYC00", "Optional feature not implemented"),
	ODBCERR("S1001", "HY001", "Memory allocation error"),
	/* TODO find best errors for ODBC version 2 */
	ODBCERR("S1000", "IM007", "No data source or driver specified"),
	ODBCERR("S1000", "08001", "Client unable to establish connection")
};

void
odbc_errs_reset(struct _sql_errors *errs)
{
	int i;

	for (i = 0; i < errs->num_errors; ++i) {
		if (errs->errs[i].msg)
			free(errs->errs[i].msg);
	}
	free(errs->errs);

	errs->errs = NULL;
	errs->num_errors = 0;
}

void
odbc_errs_add(struct _sql_errors *errs, enum _sql_error_types err_type, const char *msg)
{
	struct _sql_error *p;
	int n = errs->num_errors;

	if (errs->errs)
		p = (struct _sql_error *) realloc(errs->errs, sizeof(struct _sql_error) * (n + 1));
	else
		p = (struct _sql_error *) malloc(sizeof(struct _sql_error));
	if (!p)
		return;

	errs->errs = p;
	errs->errs[n].err = &odbc_errs[err_type];
	errs->errs[n].msg = msg ? strdup(msg) : NULL;
	++errs->num_errors;
}
