/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2017  Frediano Ziglio
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
 * Purpose: test we can declare any possible column type.
 */
#include "common.h"
#include <assert.h>

static void test_declaration(TDSSOCKET *tds, TDSCOLUMN *curcol)
{
	char declaration[128];
	TDSRET ret;

	declaration[0] = 0;
	ret = tds_get_column_declaration(tds, curcol, declaration);
	assert(ret == TDS_SUCCESS);
	printf("Declaration: %s\n", declaration);
	assert(declaration[0] != 0);
}

TEST_MAIN()
{
	int g_result = 0;
	TDSCONTEXT *ctx;
	TDSSOCKET *tds;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	ctx = tds_alloc_context(NULL);
	assert(ctx);
	if (!ctx->locale->datetime_fmt) {
		/* set default in case there's no locale file */
		ctx->locale->datetime_fmt = strdup(STD_DATETIME_FMT);
	}
	free(ctx->locale->date_fmt);
	ctx->locale->date_fmt = strdup("%Y-%m-%d");
	free(ctx->locale->time_fmt);
	ctx->locale->time_fmt = strdup("%H:%M:%S");

	tds = tds_alloc_socket(ctx, 512);
	assert(tds);

	tds_all_types(tds, test_declaration);

	tds_free_socket(tds);
	tds_free_context(ctx);

	return g_result;
}
