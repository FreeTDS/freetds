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
#endif

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <tds.h>
#include <tdsstring.h>
#include <tdssrv.h>

#ifdef __MINGW32__
#define sleep(s) Sleep((s)*1000)
#endif

static char software_version[] = "$Id: unittest.c,v 1.16 2007-03-16 15:33:26 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void dump_login(TDSLOGIN * login);

int
main(int argc, char **argv)
{
	TDSCONTEXT *ctx;
	TDSSOCKET *tds;
	TDSLOGIN *login;
	TDSRESULTINFO *resinfo;

	tds = tds_listen(atoi(argv[1]));
	/* get_incoming(tds->s); */
	login = tds_alloc_read_login(tds);
	if (!login) {
		fprintf(stderr, "Error reading login\n");
		exit(1);
	}
	dump_login(login);
	if (!strcmp(tds_dstr_cstr(&login->user_name), "guest") && !strcmp(tds_dstr_cstr(&login->password), "sybase")) {
		tds->out_flag = TDS_REPLY;
		tds_env_change(tds, TDS_ENV_DATABASE, "master", "pubs2");
		tds_send_msg(tds, 5701, 2, 10, "Changed database context to 'pubs2'.", "JDBC", "ZZZZZ", 1);
		if (!login->suppress_language) {
			tds_env_change(tds, TDS_ENV_LANG, NULL, "us_english");
			tds_send_msg(tds, 5703, 1, 10, "Changed language setting to 'us_english'.", "JDBC", "ZZZZZ", 1);
		}
		tds_env_change(tds, TDS_ENV_PACKSIZE, NULL, "512");
		/* TODO set mssql if tds7+ */
		tds_send_login_ack(tds, "sql server");
		if (IS_TDS50(tds))
			tds_send_capabilities_token(tds);
		tds_send_done_token(tds, 0, 1);
	} else {
		/* send nack before exiting */
		exit(1);
	}
	tds_flush_packet(tds);
	tds_free_login(login);
	login = NULL;
	/* printf("incoming packet %d\n", tds_read_packet(tds)); */
	printf("query : %s\n", tds_get_generic_query(tds));
	tds->out_flag = TDS_REPLY;
	resinfo = tds_alloc_results(1);
	resinfo->columns[0]->column_type = SYBVARCHAR;
	resinfo->columns[0]->column_size = 30;
	strcpy(resinfo->columns[0]->column_name, "name");
	resinfo->columns[0]->column_namelen = 4;
	resinfo->current_row = (TDS_UCHAR*) "pubs2";
	resinfo->columns[0]->column_data = resinfo->current_row;
	tds_send_result(tds, resinfo);
	tds_send_control_token(tds, 1);
	tds_send_row(tds, resinfo);
	tds_send_done_token(tds, 16, 1);
	tds_flush_packet(tds);
	sleep(30);

	tds_free_results(resinfo);
	ctx = tds->tds_ctx;
	tds_free_socket(tds);
	tds_free_context(ctx);
	
	return 0;
}

static void
dump_login(TDSLOGIN * login)
{
	printf("host %s\n", tds_dstr_cstr(&login->client_host_name));
	printf("user %s\n", tds_dstr_cstr(&login->user_name));
	printf("pass %s\n", tds_dstr_cstr(&login->password));
	printf("app  %s\n", tds_dstr_cstr(&login->app_name));
	printf("srvr %s\n", tds_dstr_cstr(&login->server_name));
	printf("vers %d.%d\n", login->major_version, login->minor_version);
	printf("lib  %s\n", tds_dstr_cstr(&login->library));
	printf("lang %s\n", tds_dstr_cstr(&login->language));
	printf("char %s\n", tds_dstr_cstr(&login->server_charset));
	printf("bsiz %d\n", login->block_size);
}
