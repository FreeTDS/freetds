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

static char software_version[] = "$Id: flags.c,v 1.2 2003-04-28 21:35:02 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static TDSLOGIN *login;
static TDSSOCKET *tds;

static void
fatal_error(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

static void
check_flags(TDSCOLINFO * curcol, int n, const char *res)
{
	char msg[256];
	char flags[256];
	int l;

	flags[0] = 0;
	if (curcol->column_nullable)
		strcat(flags, "nullable ");
	if (curcol->column_writeable)
		strcat(flags, "writable ");
	if (curcol->column_identity)
		strcat(flags, "identity ");
	if (curcol->column_key)
		strcat(flags, "key ");
	if (curcol->column_hidden)
		strcat(flags, "hidden ");
	l = strlen(flags);
	if (l)
		flags[l - 1] = 0;

	if (strcmp(flags, res) != 0) {
		sprintf(msg, "flags:%s\nwrong column %d flags", flags, n + 1);
		fatal_error(msg);
	}
}

int
main(int argc, char **argv)
{
	TDS_INT result_type;
	TDSCOLINFO *curcol;
	TDSRESULTINFO *info;
	const char *cmd;

	fprintf(stdout, "%s: Testing flags from server\n", __FILE__);
	if (try_tds_login(&login, &tds, __FILE__, 0) != TDS_SUCCEED) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}

	if (run_query(tds, "create table #tmp1 (i numeric(10,0) identity primary key, b varchar(20) null, c int not null)") !=
	    TDS_SUCCEED)
		fatal_error("creating table error");

	/* TDS 4.2 without FOR BROWSE clause seem to forget flags... */
	if (!IS_TDS42(tds)) {
		/* check select of all fields */
		cmd = "select * from #tmp1";
		fprintf(stdout, "%s: Testing query\n", cmd);
		if (tds_submit_query(tds, cmd, NULL) != TDS_SUCCEED)
			fatal_error("tds_submit_query() failed");

		if (tds_process_result_tokens(tds, &result_type) != TDS_SUCCEED)
			fatal_error("tds_process_result_tokens() failed");

		if (result_type != TDS_ROWFMT_RESULT)
			fatal_error("expected row fmt() failed");

		/* test columns results */
		if (tds->curr_resinfo != tds->res_info)
			fatal_error("wrong curr_resinfo");
		info = tds->curr_resinfo;

		if (info->num_cols != 3)
			fatal_error("wrong number or columns returned");

		check_flags(info->columns[0], 0, "identity");
		check_flags(info->columns[1], 1, "nullable writable");
		check_flags(info->columns[2], 2, "writable");

		if (tds_process_result_tokens(tds, &result_type) != TDS_SUCCEED)
			fatal_error("tds_process_result_tokens() failed");

		if (result_type != TDS_CMD_DONE)
			fatal_error("expected done failed");

		if (tds_process_result_tokens(tds, &result_type) != TDS_NO_MORE_RESULTS)
			fatal_error("tds_process_result_tokens() failed");
	}


	/* check select of 2 field */
	cmd = "select c, b from #tmp1 for browse";
	fprintf(stdout, "%s: Testing query\n", cmd);
	if (tds_submit_query(tds, cmd, NULL) != TDS_SUCCEED)
		fatal_error("tds_submit_query() failed");

	if (tds_process_result_tokens(tds, &result_type) != TDS_SUCCEED)
		fatal_error("tds_process_result_tokens() failed");

	if (result_type != TDS_ROWFMT_RESULT)
		fatal_error("expected row fmt() failed");

	/* test columns results */
	if (tds->curr_resinfo != tds->res_info)
		fatal_error("wrong curr_resinfo");
	info = tds->curr_resinfo;

	if (info->num_cols != 3)
		fatal_error("wrong number or columns returned");

	if (!IS_TDS42(tds)) {
		check_flags(info->columns[0], 0, "writable");
		check_flags(info->columns[1], 1, "nullable writable");
		check_flags(info->columns[2], 2, "writable key hidden");
	} else {
		check_flags(info->columns[0], 0, "");
		check_flags(info->columns[1], 1, "");
		check_flags(info->columns[2], 2, "key hidden");
	}

	if (tds_process_result_tokens(tds, &result_type) != TDS_SUCCEED)
		fatal_error("tds_process_result_tokens() failed");

	if (result_type != TDS_CMD_DONE)
		fatal_error("expected done failed");

	if (tds_process_result_tokens(tds, &result_type) != TDS_NO_MORE_RESULTS)
		fatal_error("tds_process_result_tokens() failed");

	try_tds_logout(login, tds, 0);
	return 0;
}
