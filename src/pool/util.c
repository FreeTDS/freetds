/* TDSPool - Connection pooling for TDS based databases
 * Copyright (C) 2001 Brian Bruns
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <ctype.h>

#include "pool.h"
#include <freetds/string.h>

void
dump_login(TDSLOGIN * login)
{
	fprintf(stderr, "host %s\n", tds_dstr_cstr(&login->client_host_name));
	fprintf(stderr, "user %s\n", tds_dstr_cstr(&login->user_name));
	fprintf(stderr, "pass %s\n", tds_dstr_cstr(&login->password));
	fprintf(stderr, "app  %s\n", tds_dstr_cstr(&login->app_name));
	fprintf(stderr, "srvr %s\n", tds_dstr_cstr(&login->server_name));
	fprintf(stderr, "vers %d.%d\n", TDS_MAJOR(login), TDS_MINOR(login));
	fprintf(stderr, "lib  %s\n", tds_dstr_cstr(&login->library));
	fprintf(stderr, "lang %s\n", tds_dstr_cstr(&login->language));
	fprintf(stderr, "char %s\n", tds_dstr_cstr(&login->server_charset));
	fprintf(stderr, "bsiz %d\n", login->block_size);
}

void
die_if(int expr, const char *msg)
{
	if (expr) {
		fprintf(stderr, "%s\n", msg);
		fprintf(stderr, "tdspool aborting!\n");
		exit(1);
	}
}

int
pool_write_all(TDS_SYS_SOCKET sock, const void *buf, size_t len)
{
	int ret;
	const unsigned char *p = (const unsigned char *) buf;

	while (len) {
		ret = WRITESOCKET(sock, p, len);
		/* write failed, cleanup member */
		if (ret <= 0) {
			return ret;
		}
		p   += ret;
		len -= ret;
	}
	return 1;
}

