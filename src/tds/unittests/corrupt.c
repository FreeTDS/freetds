/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2015  Frediano Ziglio
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

/*
 * Check sending integer till they overlap a packet.
 * This is to check if an integer can spread in multiple
 * packets. The problem raise from internal implementation
 * and how is handled the overlapping.
 */
#include "common.h"
#include <freetds/checks.h>

static const char select_query[] = "\nselect 'test'";

static void
unfinished_query_test(TDSSOCKET *tds)
{
	int char_len;
	char *buf, *p;
	int i, len;
	union {
		TDS_USMALLINT si;
		TDS_UINT i;
		TDS_INT8 i8;
		char buf[8];
	} conv;

	if (IS_TDS72_PLUS(tds->conn))
		return;

	tds_init_write_buf(tds);

	/* try to build an invalid (unfinished) query split in two packets */
	char_len = IS_TDS7_PLUS(tds->conn) ? 2 : 1;
	buf = tds_new0(char, tds->out_buf_max + 200);
	memset(buf, '-', tds->out_buf_max + 200);
	strcpy(buf + (tds->out_buf_max - 8) / char_len - strlen(select_query) + 1, select_query);
	memset(strchr(buf, 0), 0, 16);

	/* convert if needed */
	len = (int) strlen(buf);
	for (i = len; --i >= 0; ) {
		char c = buf[i];
		buf[i * char_len + 0] = c;
		if (IS_TDS7_PLUS(tds->conn))
			buf[i * char_len + 1] = 0;
	}
	len *= char_len;

	/* send the query using tds_put_int8, not aligned */
	tds->out_flag = TDS_QUERY;
	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		exit(1);
	p = buf;
	memcpy(conv.buf, p, 2);
#ifdef WORDS_BIGENDIAN
	tds_swap_bytes(conv.buf, 2);
#endif
	tds_put_smallint(tds, conv.si);
	p += 2;
	for (; p < buf + len; p += 8) {
		CHECK_TDS_EXTRA(tds);
		memcpy(conv.buf, p, 8);
#ifdef WORDS_BIGENDIAN
		tds_swap_bytes(conv.buf, 8);
#endif
		tds_put_int8(tds, conv.i8);
	}
	tds_flush_packet(tds);
	tds_set_state(tds, TDS_PENDING);

	/* check result was fine */
	if (TDS_FAILED(tds_process_simple_query(tds))) {
		fprintf(stderr, "Error in prepared query\n");
		exit(1);
	}
	free(buf);
}

TEST_MAIN()
{
	TDSLOGIN *login;
	TDSSOCKET *tds;
	int ret;
	int verbose = 0;
	TDS_INT8 i8;
	unsigned limit;

	printf("%s: Testing login, logout\n", __FILE__);
	ret = try_tds_login(&login, &tds, __FILE__, verbose);
	if (ret != TDS_SUCCESS) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}

	unfinished_query_test(tds);

	tds->out_flag = TDS_QUERY;
	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING) {
		return 1;
	}

	tds_put_n(tds, "aaa", 3);
	limit = tds->out_buf_max / 8 + 100;
	for (i8 = 0; i8 < limit; ++i8) {
		CHECK_TDS_EXTRA(tds);
		tds_put_int8(tds, i8);
	}

	tds_send_cancel(tds);
	tds_process_simple_query(tds);

	try_tds_logout(login, tds, verbose);
	return 0;
}
