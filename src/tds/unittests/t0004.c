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

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <tds.h>
#include "common.h"

static char software_version[] = "$Id: t0004.c,v 1.9 2002-11-20 13:34:49 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int run_query(TDSSOCKET * tds, const char *query);
char *varchar_as_string(TDSSOCKET * tds, int col_idx);

char *
varchar_as_string(TDSSOCKET * tds, int col_idx)
{
	static char result[256];
	const char *row = tds->res_info->current_row;
	const int offset = tds->res_info->columns[col_idx]->column_offset;
	const void *value = (row + offset);

	strncpy(result, (const char *) value, sizeof(result) - 1);
	result[sizeof(result) - 1] = '\0';
	return result;
}


int
main(int argc, char **argv)
{
	TDSLOGIN *login;
	TDSSOCKET *tds;
	int verbose = 0;
	int rc;
	int i;

	int result_type;
	int row_type;
	int compute_id;

	const char *len200 =
		"01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
	char long_query[1000];

	sprintf(long_query,
		"SELECT name FROM #longquerytest WHERE (name = 'A%s' OR name = 'B%s' OR name = 'C%s' OR name = 'correct')", len200,
		len200, len200);

	fprintf(stdout, "%s: Test large (>512 bytes) queries\n", __FILE__);
	rc = try_tds_login(&login, &tds, __FILE__, verbose);
	if (rc != TDS_SUCCEED) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}

	/* do not check error here, if TABLE is not create this give error */
	rc = run_query(tds, "DROP TABLE #longquerytest");
	rc = run_query(tds, "CREATE TABLE #longquerytest (name varchar(255))");
	if (rc != TDS_SUCCEED) {
		return 1;
	}
	rc = run_query(tds, "INSERT #longquerytest (name) VALUES ('incorrect')");
	if (rc != TDS_SUCCEED) {
		return 1;
	}
	rc = run_query(tds, "INSERT #longquerytest (name) VALUES ('correct')");
	if (rc != TDS_SUCCEED) {
		return 1;
	}

	/*
	 * The heart of the test
	 */
	if (verbose) {
		fprintf(stdout, "block size %d\n", tds->env->block_size);
	}
	rc = tds_submit_query(tds, long_query);
	while ((rc = tds_process_result_tokens(tds, &result_type)) == TDS_SUCCEED) {
		switch (result_type) {
		case TDS_ROWFMT_RESULT:
			if (tds->res_info->columns[0]->column_type != SYBVARCHAR) {
				fprintf(stderr, "Wrong column_type in %s\n", __FILE__);
				return 1;
			}
			break;
		case TDS_ROW_RESULT:
			while ((rc = tds_process_row_tokens(tds, &row_type, &compute_id)) == TDS_SUCCEED) {
				if (verbose) {
					printf("col %i is %s\n", i, varchar_as_string(tds, 0));
				}
			}
			if (rc == TDS_FAIL) {
				fprintf(stderr, "tds_process_row_tokens() returned TDS_FAIL\n");
				return 1;
			} else if (rc != TDS_NO_MORE_ROWS) {
				fprintf(stderr, "tds_process_row_tokens() unexpected return\n");
				return 1;
			}
			break;
		default:
			break;
		}
	}
	if (rc == TDS_FAIL) {
		fprintf(stderr, "tds_process_result_tokens() returned TDS_FAIL for long query\n");
		return 1;
	} else if (rc != TDS_NO_MORE_RESULTS) {
		fprintf(stderr, "tds_process_result_tokens() unexpected return\n");
	}

	/* do not check error here, if TABLE is not create this give error */
	rc = run_query(tds, "DROP TABLE #longquerytest");

	try_tds_logout(login, tds, verbose);
	return 0;
}


/* Run query for which there should be no return results */
int
run_query(TDSSOCKET * tds, const char *query)
{
	int rc;
	int result_type;

	rc = tds_submit_query(tds, query);
	if (rc != TDS_SUCCEED) {
		fprintf(stderr, "tds_submit_query() failed for query '%s'\n", query);
		return TDS_FAIL;
	}

	while ((rc = tds_process_result_tokens(tds, &result_type)) == TDS_SUCCEED) {

		if (result_type != TDS_CMD_DONE && result_type != TDS_CMD_SUCCEED && result_type != TDS_CMD_FAIL) {
			fprintf(stderr, "Error:  query should not return results\n");
			return TDS_FAIL;
		}
	}
	if (rc == TDS_FAIL) {
		/* probably okay - DROP TABLE might cause this */
		/* fprintf(stderr, "tds_process_result_tokens() returned TDS_FAIL for '%s'\n", query); */
	} else if (rc != TDS_NO_MORE_RESULTS) {
		fprintf(stderr, "tds_process_result_tokens() unexpected return\n");
		return TDS_FAIL;
	}

	return TDS_SUCCEED;
}
