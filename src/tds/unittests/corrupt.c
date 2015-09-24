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

int
main(int argc, char **argv)
{
	TDSLOGIN *login;
	TDSSOCKET *tds;
	int ret;
	int verbose = 0;
	TDS_INT8 i8;
	unsigned limit;

	fprintf(stdout, "%s: Testing login, logout\n", __FILE__);
	ret = try_tds_login(&login, &tds, __FILE__, verbose);
	if (ret != TDS_SUCCESS) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}

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
