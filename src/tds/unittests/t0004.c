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
#include "common.h"

static char software_version[] = "$Id: t0004.c,v 1.16 2003-07-23 18:34:40 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

char *varchar_as_string(TDSSOCKET * tds, int col_idx);

char *
varchar_as_string(TDSSOCKET * tds, int col_idx)
{
	static char result[256];
	const unsigned char *row = tds->res_info->current_row;
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

	int result_type;
	int row_type;
	int compute_id;
	int rows_returned = 0;

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
	rc = tds_submit_query(tds, long_query, NULL);
	while ((rc = tds_process_result_tokens(tds, &result_type, NULL)) == TDS_SUCCEED) {
		switch (result_type) {
		case TDS_ROWFMT_RESULT:
			if (tds->res_info->columns[0]->column_type != SYBVARCHAR) {
				fprintf(stderr, "Wrong column_type in %s\n", __FILE__);
				return 1;
			}
			break;
		case TDS_ROW_RESULT:
			while ((rc = tds_process_row_tokens(tds, &row_type, &compute_id)) == TDS_SUCCEED) {
				++rows_returned;
				if (verbose) {
					printf("col 0 is %s\n", varchar_as_string(tds, 0));
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

	if (rows_returned != 1) {
		fprintf(stderr, "%d rows returned, 1 expected\n", rows_returned);
		return 1;
	}

	/* do not check error here, if TABLE is not create this give error */
	rc = run_query(tds, "DROP TABLE #longquerytest");

	try_tds_logout(login, tds, verbose);
	return 0;
}
