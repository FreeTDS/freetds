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
#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#include "tds.h"
#include "tdsiconv.h"
#include "tdssrv.h"
#include "tdsstring.h"

static char software_version[] = "$Id: login.c,v 1.35 2003-12-29 22:37:42 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

unsigned char *
tds7_decrypt_pass(const unsigned char *crypt_pass, int len, unsigned char *clear_pass)
{
	int i;
	const unsigned char xormask = 0x5A;
	unsigned char hi_nibble, lo_nibble;

	for (i = 0; i < len; i++) {
		lo_nibble = (crypt_pass[i] << 4) ^ (xormask & 0xF0);
		hi_nibble = (crypt_pass[i] >> 4) ^ (xormask & 0x0F);
		clear_pass[i] = hi_nibble | lo_nibble;
	}
	return clear_pass;
}

TDSSOCKET *
tds_listen(int ip_port)
{
	TDSCONTEXT *context;
	TDSSOCKET *tds;
	struct sockaddr_in sin;
	TDS_SYS_SOCKET fd, s;
	socklen_t len;

	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((short) ip_port);
	sin.sin_family = AF_INET;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		perror("bind");
		exit(1);
	}
	listen(s, 5);
	if ((fd = accept(s, (struct sockaddr *) &sin, &len)) < 0) {
		perror("accept");
		exit(1);
	}
	context = tds_alloc_context();
	tds = tds_alloc_socket(context, 8192);
	tds->s = fd;
	tds->out_flag = 0x02;
	/* get_incoming(tds->s); */
	return tds;
}

static char *tds_read_string(TDSSOCKET * tds, DSTR * s, int size);

void
tds_read_login(TDSSOCKET * tds, TDSLOGIN * login)
{
	DSTR blockstr;

/*
	while (len = tds_read_packet(tds)) {
		for (i=0;i<len;i++)
			printf("%d %d %c\n",i, tds->in_buf[i], (tds->in_buf[i]>=' ' && tds->in_buf[i]<='z') ? tds->in_buf[i] : ' ');
	}	
*/
	tds_dstr_init(&blockstr);
	tds_read_string(tds, &login->host_name, 30);
	tds_read_string(tds, &login->user_name, 30);
	tds_read_string(tds, &login->password, 30);
	tds_get_n(tds, NULL, 31);	/* host process, junk for now */
	tds_get_n(tds, NULL, 16);	/* magic */
	tds_read_string(tds, &login->app_name, 30);
	tds_read_string(tds, &login->server_name, 30);
	tds_get_n(tds, NULL, 256);	/* secondary passwd...encryption? */
	login->major_version = tds_get_byte(tds);
	login->minor_version = tds_get_byte(tds);
	tds_get_smallint(tds);	/* unused part of protocol field */
	tds_read_string(tds, &login->library, 10);
	tds_get_byte(tds);	/* program version, junk it */
	tds_get_byte(tds);
	tds_get_smallint(tds);
	tds_get_n(tds, NULL, 3);	/* magic */
	tds_read_string(tds, &login->language, 30);
	tds_get_n(tds, NULL, 14);	/* magic */
	tds_read_string(tds, &login->server_charset, 30);
	tds_get_n(tds, NULL, 1);	/* magic */
	tds_read_string(tds, &blockstr, 6);
	printf("block size %s\n", tds_dstr_cstr(&blockstr));
	login->block_size = atoi(tds_dstr_cstr(&blockstr));
	tds_dstr_free(&blockstr);
	tds_get_n(tds, NULL, tds->in_len - tds->in_pos);	/* read junk at end */
}

static char *
tds7_read_string(TDSSOCKET * tds, int len)
{
	char *s;

	s = (char *) malloc(len + 1);
	/* FIXME possible truncation on char conversion ? */
	len = tds_get_string(tds, len, s, len);
	s[len] = 0;
	return s;

}

int
tds7_read_login(TDSSOCKET * tds, TDSLOGIN * login)
{
	int a;
	int host_name_len, user_name_len, app_name_len, server_name_len;
	int library_name_len, language_name_len;
	size_t unicode_len, password_len;
	char *unicode_string;
	char *buf, *pbuf;

	a = tds_get_smallint(tds);	/*total packet size */
	tds_get_n(tds, NULL, 5);
	a = tds_get_byte(tds);	/*TDS version */
	login->major_version = a >> 4;
	login->minor_version = a << 4;
	tds_get_n(tds, NULL, 3);	/*rest of TDS Version which is a 4 byte field */
	tds_get_n(tds, NULL, 4);	/*desired packet size being requested by client */
	tds_get_n(tds, NULL, 21);	/*magic1 */
	a = tds_get_smallint(tds);	/*current position */
	host_name_len = tds_get_smallint(tds);
	a = tds_get_smallint(tds);	/*current position */
	user_name_len = tds_get_smallint(tds);
	a = tds_get_smallint(tds);	/*current position */
	password_len = tds_get_smallint(tds);
	a = tds_get_smallint(tds);	/*current position */
	app_name_len = tds_get_smallint(tds);
	a = tds_get_smallint(tds);	/*current position */
	server_name_len = tds_get_smallint(tds);
	tds_get_smallint(tds);
	tds_get_smallint(tds);
	a = tds_get_smallint(tds);	/*current position */
	library_name_len = tds_get_smallint(tds);
	a = tds_get_smallint(tds);	/*current position */
	language_name_len = tds_get_smallint(tds);
	tds_get_smallint(tds);
	tds_get_smallint(tds);
	tds_get_n(tds, NULL, 6);	/*magic2 */
	a = tds_get_smallint(tds);	/*partial packet size */
	a = tds_get_smallint(tds);	/*0x30 */
	a = tds_get_smallint(tds);	/*total packet size */
	tds_get_smallint(tds);

	tds_dstr_set(&login->host_name, tds7_read_string(tds, host_name_len));
	tds_dstr_set(&login->user_name, tds7_read_string(tds, user_name_len));

	unicode_len = password_len * 2;
	unicode_string = (char *) malloc(unicode_len);
	buf = (char *) malloc(password_len + 1);
	tds_get_n(tds, unicode_string, unicode_len);
	tds7_decrypt_pass((unsigned char *) unicode_string, unicode_len, (unsigned char *) unicode_string);
	pbuf = buf;
	
	memset(&tds->iconv_info[client2ucs2]->suppress, 0, sizeof(tds->iconv_info[client2ucs2]->suppress));
	a = tds_iconv(tds, tds->iconv_info[client2ucs2], to_client, (const char **) &unicode_string, &unicode_len, &pbuf,
			 &password_len);
	if (a < 0 ) {
		fprintf(stderr, "error: %s:%d: tds7_read_login: tds_iconv() failed\n", __FILE__, __LINE__);
		assert(-1 != a);
	}
	*pbuf = '\0';
	tds_dstr_set(&login->password, buf);
	free(unicode_string);

	tds_dstr_set(&login->app_name, tds7_read_string(tds, app_name_len));
	tds_dstr_set(&login->server_name, tds7_read_string(tds, server_name_len));
	tds_dstr_set(&login->library, tds7_read_string(tds, library_name_len));
	tds_dstr_set(&login->language, tds7_read_string(tds, language_name_len));

	tds_get_n(tds, NULL, 7);	/*magic3 */
	tds_get_byte(tds);
	tds_get_byte(tds);
	tds_get_n(tds, NULL, 3);
	tds_get_byte(tds);
	a = tds_get_byte(tds);	/*0x82 */
	tds_get_n(tds, NULL, 22);
	tds_get_byte(tds);	/*0x30 */
	tds_get_n(tds, NULL, 7);
	a = tds_get_byte(tds);	/*0x30 */
	tds_get_n(tds, NULL, 3);
	tds_dstr_copy(&login->server_charset, "");	/*empty char_set for TDS 7.0 */
	login->block_size = 0;	/*0 block size for TDS 7.0 */
	login->encrypted = 1;
	return (0);

}
static char *
tds_read_string(TDSSOCKET * tds, DSTR * s, int size)
{
	char *tempbuf;
	int len;

	tempbuf = (char *) malloc(size + 1);
	tds_get_n(tds, tempbuf, size);
	tempbuf[size] = 0;
	len = tds_get_byte(tds);
	if (len <= size)
		tempbuf[len] = '\0';

	tds_dstr_set(s, tempbuf);
	return tempbuf;
}
