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

#include <ctype.h>
#include <assert.h>

static char software_version[] = "$Id: utf8_1.c,v 1.2 2003-11-15 09:30:45 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static TDSSOCKET *tds;

/* Some no-ASCII strings (XML coding) */
static const char english[] = "English";
static const char spanish[] = "Espa&#241;ol";
static const char french[] = "Fran&#231;ais";
static const char portuguese[] = "Portugu&#234;s";
static const char russian[] = "&#1056;&#1091;&#1089;&#1089;&#1082;&#1080;&#1081;";
static const char arabic[] = "&#x0627;&#x0644;&#x0639;&#x0631;&#x0628;&#x064a;&#x0629;";
static const char chinese[] = "&#x7b80;&#x4f53;&#x4e2d;&#x6587;";
static const char japanese[] = "&#26085;&#26412;&#35486;";
static const char hebrew[] = "&#x05e2;&#x05d1;&#x05e8;&#x05d9;&#x05ea;";

static const char *strings[] = {
	english,
	spanish,
	french,
	portuguese,
	russian,
	arabic,
	chinese,
	japanese,
	hebrew,
	NULL
};

static int max_len = 0;

static char *
to_utf8(const char *src, char *dest)
{
	unsigned char *p = (unsigned char *) dest;
	int len = 0;

	for (; *src;) {
		if (src[0] == '&' && src[1] == '#') {
			const char *end = strchr(src, ';');
			char tmp[16];
			int radix = 10;
			int n;

			assert(end);
			src += 2;
			if (toupper(*src) == 'X') {
				radix = 16;
				++src;
			}
			memcpy(tmp, src, end - src);
			tmp[end - src] = 0;
			n = strtol(tmp, NULL, radix);
			assert(n > 0 && n < 0x10000);
			if (n >= 0x1000) {
				*p++ = 0xe0 | (n >> 12);
				*p++ = 0x80 | ((n >> 6) & 0x3f);
				*p++ = 0x80 | (n & 0x3f);
			} else if (n >= 0x80) {
				*p++ = 0xc0 | (n >> 6);
				*p++ = 0x80 | (n & 0x3f);
			} else {
				*p++ = (unsigned char) n;
			}
			src = end + 1;
		} else {
			*p++ = *src++;
		}
		++len;
	}
	if (len > max_len)
		max_len = len;
	*p = 0;
	return dest;
}

static void
query(const char *sql)
{
	if (run_query(tds, sql) != TDS_SUCCEED) {
		fprintf(stderr, "error executing query: %s\n", sql);
		exit(1);
	}
}

static void
test(const char *type, const char *test_name)
{
	char buf[256];
	char tmp[256];
	int i;
	const char **s;
	int rc;
	TDS_INT result_type;
	TDS_INT row_type;
	TDS_INT compute_id;
	int done_flags;

	sprintf(buf, "CREATE TABLE #tmp (i INT, t %s)", type);
	query(buf);

	/* insert all test strings in table */
	for (i = 0, s = strings; *s; ++s, ++i) {
		sprintf(buf, "insert into #tmp values(%d, N'%s')", i, to_utf8(*s, tmp));
		query(buf);
	}

	/* do a select and check all results */
	rc = tds_submit_query(tds, "select t from #tmp order by i");
	if (rc != TDS_SUCCEED) {
		fprintf(stderr, "tds_submit_query() failed\n");
		exit(1);
	}

	if (tds_process_result_tokens(tds, &result_type, NULL) != TDS_SUCCEED) {
		fprintf(stderr, "tds_process_result_tokens() failed\n");
		exit(1);
	}

	if (result_type != TDS_ROWFMT_RESULT) {
		fprintf(stderr, "expected row fmt() failed\n");
		exit(1);
	}

	if (tds_process_result_tokens(tds, &result_type, NULL) != TDS_SUCCEED) {
		fprintf(stderr, "tds_process_result_tokens() failed\n");
		exit(1);
	}

	if (result_type != TDS_ROW_RESULT) {
		fprintf(stderr, "expected row result() failed\n");
		exit(1);
	}

	i = 0;
	while ((rc = tds_process_row_tokens(tds, &row_type, &compute_id)) == TDS_SUCCEED) {

		TDSCOLINFO *curcol = tds->curr_resinfo->columns[0];
		unsigned char *src = tds->curr_resinfo->current_row + curcol->column_offset;

		if (is_blob_type(curcol->column_type)) {
			TDSBLOBINFO *bi = (TDSBLOBINFO *) src;

			src = (unsigned char *) bi->textvalue;
		}

		strcpy(buf, to_utf8(strings[i], tmp));

		if (strlen(buf) != curcol->column_cur_size || strncmp(buf, src, curcol->column_cur_size) != 0) {
			int l = curcol->column_cur_size;

			if (l > 200)
				l = 200;
			strncpy(tmp, src, l);
			tmp[l] = 0;
			fprintf(stderr, "Wrong result in test %s\n Got: '%s' len %d\n Expected: '%s' len %d\n", test_name, tmp,
				curcol->column_cur_size, buf, strlen(buf));
			exit(1);
		}
		++i;
	}

	if (rc != TDS_NO_MORE_ROWS) {
		fprintf(stderr, "tds_process_row_tokens() unexpected return\n");
		exit(1);
	}

	while ((rc = tds_process_result_tokens(tds, &result_type, &done_flags)) == TDS_SUCCEED) {
		switch (result_type) {
		case TDS_NO_MORE_RESULTS:
			return;

		case TDS_DONE_RESULT:
		case TDS_DONEPROC_RESULT:
		case TDS_DONEINPROC_RESULT:
			if (!(done_flags & TDS_DONE_ERROR))
				break;

		default:
			fprintf(stderr, "tds_process_result_tokens() unexpected result_type\n");
			exit(1);
			break;
		}
	}

	query("DROP TABLE #tmp");

	/* do sone select to test results */
	/*
	 * for (s = strings; *s; ++s) {
	 * printf("%s\n", to_utf8(*s, tmp));
	 * }
	 */
}

int
main(int argc, char **argv)
{
	TDSLOGIN *login;
	int ret;
	int verbose = 0;

	/* use UTF-8 as our coding */
	strcpy(CHARSET, "UTF-8");

	ret = try_tds_login(&login, &tds, __FILE__, verbose);
	if (ret != TDS_SUCCEED) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}

	if (IS_TDS7_PLUS(tds)) {
		char type[32];

		test("NVARCHAR(40)", "NVARCHAR with large size");

		sprintf(type, "NVARCHAR(%d)", max_len);
		test(type, "NVARCHAR with sufficient size");

		test("TEXT", "TEXT");

		/* TODO test parameters */
	}

	try_tds_logout(login, tds, verbose);
	return 0;
}
