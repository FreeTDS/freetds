/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2003  Brian Bruns
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

#include <tdsconvert.h>

static char software_version[] = "$Id: dataread.c,v 1.4 2003-05-22 19:05:11 castellano Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int g_result = 0;
static TDSLOGIN *login;
static TDSSOCKET *tds;

void test(const char *type, const char *value, const char *result);

void
test(const char *type, const char *value, const char *result)
{
	char buf[512];
	CONV_RESULT cr;
	int rc;
	TDS_INT result_type;
	TDS_INT row_type;
	TDS_INT compute_id;

	if (!result)
		result = value;

	/* build select */
	sprintf(buf, "SELECT CONVERT(%s,'%s')", type, value);

	/* execute it */
	rc = tds_submit_query(tds, buf, NULL);
	if (rc != TDS_SUCCEED) {
		fprintf(stderr, "tds_submit_query() failed\n");
		exit(1);
	}

	if (tds_process_result_tokens(tds, &result_type) != TDS_SUCCEED) {
		fprintf(stderr, "tds_process_result_tokens() failed\n");
		exit(1);
	}

	if (result_type != TDS_ROWFMT_RESULT) {
		fprintf(stderr, "expected row fmt() failed\n");
		exit(1);
	}

	if (tds_process_result_tokens(tds, &result_type) != TDS_SUCCEED) {
		fprintf(stderr, "tds_process_result_tokens() failed\n");
		exit(1);
	}

	if (result_type != TDS_ROW_RESULT) {
		fprintf(stderr, "expected row result() failed\n");
		exit(1);
	}

	while ((rc = tds_process_row_tokens(tds, &row_type, &compute_id)) == TDS_SUCCEED) {

		TDSCOLINFO *curcol = tds->curr_resinfo->columns[0];
		unsigned char *src = tds->curr_resinfo->current_row + curcol->column_offset;
		int conv_type = tds_get_conversion_type(curcol->column_type, curcol->column_size);

		if (is_blob_type(curcol->column_type)) {
			TDSBLOBINFO *bi = (TDSBLOBINFO *) src;

			src = (unsigned char *) bi->textvalue;
		}

		if (tds_convert(test_context, conv_type, src, curcol->column_cur_size, SYBVARCHAR, &cr) < 0) {
			fprintf(stderr, "Error converting\n");
			g_result = 1;
		} else {
			if (strcmp(result, cr.c) != 0) {
				fprintf(stderr, "Failed! Is \n%s\nShould be\n%s\n", cr.c, result);
				g_result = 1;
			}
			free(cr.c);
		}
	}

	if (rc != TDS_NO_MORE_ROWS) {
		fprintf(stderr, "tds_process_row_tokens() unexpected return\n");
		exit(1);
	}

	while ((rc = tds_process_result_tokens(tds, &result_type)) == TDS_SUCCEED) {
		switch (result_type) {
		case TDS_CMD_DONE:
			break;

		case TDS_NO_MORE_RESULTS:
			return;

		default:
			fprintf(stderr, "tds_process_result_tokens() unexpected result_type\n");
			exit(1);
			break;
		}
	}
}

int
main(int argc, char **argv)
{
	fprintf(stdout, "%s: Testing conversion from server\n", __FILE__);
	if (try_tds_login(&login, &tds, __FILE__, 0) != TDS_SUCCEED) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}

	/* bit */
	test("BIT", "0", NULL);
	test("BIT", "1", NULL);

	/* integers */
	test("TINYINT", "234", NULL);
	test("SMALLINT", "-31789", NULL);
	test("INT", "16909060", NULL);

	/* floating point */
	test("REAL", "1.23", NULL);
	test("FLOAT", "-49586.345", NULL);

	/* money */
	test("MONEY", "-123.3400", NULL);
	test("SMALLMONEY", "89123.12", NULL);

	/* char */
	test("CHAR(10)", "pippo", "pippo     ");
	test("VARCHAR(20)", "pippo", NULL);
	test("TEXT", "foofoo", NULL);

	/* binary */
	test("VARBINARY(6)", "foo", "666f6f");
	test("BINARY(6)", "foo", "666f6f000000");
	test("IMAGE", "foo", "666f6f");

	/* numeric */
	test("NUMERIC(10,2)", "12765.76", NULL);
	test("NUMERIC(18,4)", "12765.761234", "12765.7612");

	/* date */
	if (test_context->locale->date_fmt)
		free(test_context->locale->date_fmt);
	test_context->locale->date_fmt = strdup("%Y-%m-%d %H:%M:%S");

	test("DATETIME", "2003-04-21 17:50:03", NULL);
	test("SMALLDATETIME", "2003-04-21 17:50:03", "2003-04-21 17:50:00");

	if (IS_TDS7_PLUS(tds)) {
		test("UNIQUEIDENTIFIER", "12345678-1234-A234-9876-543298765432", NULL);
		test("NVARCHAR(20)", "Excellent test", NULL);
		test("NCHAR(20)", "Excellent test", "Excellent test      ");
		test("NTEXT", "Excellent test", NULL);
	}

	try_tds_logout(login, tds, 0);
	return g_result;
}
