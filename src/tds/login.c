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

#if TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

#include "tds.h"
#include "tdsstring.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: login.c,v 1.77 2002-12-25 11:19:32 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int tds_send_login(TDSSOCKET * tds, TDSCONNECTINFO * connect_info);
static int tds7_send_login(TDSSOCKET * tds, TDSCONNECTINFO * connect_info);

void
tds_set_version(TDSLOGIN * tds_login, short major_ver, short minor_ver)
{
	tds_login->major_version = major_ver;
	tds_login->minor_version = minor_ver;
}

void
tds_set_packet(TDSLOGIN * tds_login, int packet_size)
{
	tds_login->block_size = packet_size;
}

void
tds_set_port(TDSLOGIN * tds_login, int port)
{
	tds_login->port = port;
}

void
tds_set_passwd(TDSLOGIN * tds_login, const char *password)
{
	if (password) {
		tds_dstr_zero(&tds_login->password);
		tds_dstr_copy(&tds_login->password, password);
	}
}
void
tds_set_bulk(TDSLOGIN * tds_login, TDS_TINYINT enabled)
{
	tds_login->bulk_copy = enabled ? 0 : 1;
}

void
tds_set_user(TDSLOGIN * tds_login, const char *username)
{
	tds_dstr_copy(&tds_login->user_name, username);
}

void
tds_set_host(TDSLOGIN * tds_login, const char *hostname)
{
	tds_dstr_copy(&tds_login->host_name, hostname);
}

void
tds_set_app(TDSLOGIN * tds_login, const char *application)
{
	tds_dstr_copy(&tds_login->app_name, application);
}

void
tds_set_server(TDSLOGIN * tds_login, const char *server)
{
	if (!server || strlen(server) == 0) {
		server = getenv("DSQUERY");
		if (!server || strlen(server) == 0) {
			server = "SYBASE";
		}
	}
	tds_dstr_copy(&tds_login->server_name, server);
}

void
tds_set_library(TDSLOGIN * tds_login, const char *library)
{
	tds_dstr_copy(&tds_login->library, library);
}

void
tds_set_charset(TDSLOGIN * tds_login, const char *charset)
{
	tds_dstr_copy(&tds_login->char_set, charset);
}

void
tds_set_language(TDSLOGIN * tds_login, const char *language)
{
	tds_dstr_copy(&tds_login->language, language);
}

/* Jeffs' hack to support timeouts */
void
tds_set_timeouts(TDSLOGIN * tds_login, int connect_timeout, int query_timeout, int longquery_timeout)
{
	tds_login->connect_timeout = connect_timeout;
	tds_login->query_timeout = query_timeout;
	tds_login->longquery_timeout = longquery_timeout;
}

void
tds_set_longquery_handler(TDSLOGIN * tds_login, void (*longquery_func) (long), long longquery_param)
{				/* Jeff's hack */
	tds_login->longquery_func = longquery_func;
	tds_login->longquery_param = longquery_param;
}

void
tds_set_capabilities(TDSLOGIN * tds_login, unsigned char *capabilities, int size)
{
	memcpy(tds_login->capabilities, capabilities, size > TDS_MAX_CAPABILITY ? TDS_MAX_CAPABILITY : size);
}

int
tds_connect(TDSSOCKET * tds, TDSCONNECTINFO * connect_info)
{
	struct sockaddr_in sin;

	/* Jeff's hack - begin */
	unsigned long ioctl_blocking = 1;
	struct timeval selecttimeout;
	fd_set fds;
	int retval;
	time_t start, now;
	int connect_timeout = 0;
	int result_type;
	int db_selected = 0;
	char version[256];

	FD_ZERO(&fds);

	/*
	 * If a dump file has been specified, start logging
	 */
	if (!tds_dstr_isempty(&connect_info->dump_file)) {
		tdsdump_open(connect_info->dump_file);
	}

	tds->connect_info = connect_info;

	tds->major_version = connect_info->major_version;
	tds->minor_version = connect_info->minor_version;
	tds->emul_little_endian = connect_info->emul_little_endian;
#ifdef WORDS_BIGENDIAN
	if (IS_TDS70(tds) || IS_TDS80(tds)) {
		/* TDS 7/8 only supports little endian */
		tds->emul_little_endian = 1;
	}
#endif

	/* set up iconv */
	if (connect_info->client_charset) {
		tds_iconv_open(tds, connect_info->client_charset);
	}

	/* specified a date format? */
	/*
	 * if (connect_info->date_fmt) {
	 * tds->date_fmt=strdup(connect_info->date_fmt);
	 * }
	 */
	connect_timeout = connect_info->connect_timeout;

	/* Jeff's hack - begin */
	tds->timeout = (connect_timeout) ? connect_info->query_timeout : 0;
	tds->longquery_timeout = (connect_timeout) ? connect_info->longquery_timeout : 0;
	tds->longquery_func = connect_info->longquery_func;
	tds->longquery_param = connect_info->longquery_param;
	/* end */

	/* verify that ip_addr is not NULL */
	if (tds_dstr_isempty(&connect_info->ip_addr)) {
		tdsdump_log(TDS_DBG_ERROR, "%L IP address pointer is NULL\n");
		if (connect_info->server_name) {
			tdsdump_log(TDS_DBG_ERROR, "%L Server %s not found!\n", connect_info->server_name);
		} else {
			tdsdump_log(TDS_DBG_ERROR, "%L No server specified!\n");
		}
		tds_free_socket(tds);
		return TDS_FAIL;
	}
	sin.sin_addr.s_addr = inet_addr(connect_info->ip_addr);
	if (sin.sin_addr.s_addr == -1) {
		tdsdump_log(TDS_DBG_ERROR, "%L inet_addr() failed, IP = %s\n", connect_info->ip_addr);
		tds_free_socket(tds);
		return TDS_FAIL;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(connect_info->port);

	memcpy(tds->capabilities, connect_info->capabilities, TDS_MAX_CAPABILITY);


	retval = tds_version(tds, version);
	if (!retval)
		version[0] = '\0';

	tdsdump_log(TDS_DBG_INFO1, "%L Connecting addr %s port %d with TDS version %s\n", inet_ntoa(sin.sin_addr),
		    ntohs(sin.sin_port), version);
	if ((tds->s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		tds_free_socket(tds);
		return TDS_FAIL;
	}

	/* Jeff's hack *** START OF NEW CODE *** */
	if (connect_timeout) {
		start = time(NULL);
		ioctl_blocking = 1;	/* ~0; //TRUE; */
		if (IOCTLSOCKET(tds->s, FIONBIO, &ioctl_blocking) < 0) {
			tds_free_socket(tds);
			return TDS_FAIL;
		}
		retval = connect(tds->s, (struct sockaddr *) &sin, sizeof(sin));
		if (retval < 0 && errno == EINPROGRESS)
			retval = 0;
		if (retval < 0) {
			perror("src/tds/login.c: tds_connect (timed)");
			tds_free_socket(tds);
			return TDS_FAIL;
		}
		/* Select on writeability for connect_timeout */
		now = start;
		while ((retval == 0) && ((now - start) < connect_timeout)) {
			FD_SET(tds->s, &fds);
			selecttimeout.tv_sec = connect_timeout - (now - start);
			selecttimeout.tv_usec = 0;
			retval = select(tds->s + 1, NULL, &fds, NULL, &selecttimeout);
			if (retval < 0 && errno == EINTR)
				retval = 0;
			now = time(NULL);
		}

		if ((now - start) >= connect_timeout) {
			tds_client_msg(tds->tds_ctx, tds, 20009, 9, 0, 0, "Server is unavailable or does not exist.");
			tds_free_socket(tds);
			return TDS_FAIL;
		}
	} else if (connect(tds->s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		char *message;

		if (asprintf(&message, "src/tds/login.c: tds_connect: %s:%d", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port)) >= 0) {
			perror(message);
			free(message);
		}
		tds_client_msg(tds->tds_ctx, tds, 20009, 9, 0, 0, "Server is unavailable or does not exist.");
		tds_free_socket(tds);
		return TDS_FAIL;
	}
	/* END OF NEW CODE */

	if (IS_TDS7_PLUS(tds)) {
		tds->out_flag = 0x10;
		tds7_send_login(tds, connect_info);
		db_selected = 1;
	} else {
		tds->out_flag = 0x02;
		tds_send_login(tds, connect_info);
	}
	if (!tds_process_login_tokens(tds)) {
		tds_close_socket(tds);
		tds_client_msg(tds->tds_ctx, tds, 20014, 9, 0, 0, "Login incorrect.");
		tds_free_socket(tds);
		return TDS_FAIL;
	}
	if (tds && connect_info->text_size) {
		retval = tds_submit_queryf(tds, "set textsize %d", connect_info->text_size);
		if (retval == TDS_SUCCEED) {
			/* TODO a function is suitable */
			while (tds_process_result_tokens(tds, &result_type) == TDS_SUCCEED);
		}
	}

	/* TODO set also database */
	if (!db_selected) {
	}

	tds->connect_info = NULL;
	return TDS_SUCCEED;
}

static int
tds_put_login_string(TDSSOCKET * tds, const char *buf, int n)
{
	int buf_len = (buf ? strlen(buf) : 0);

	return tds_put_buf(tds, (const unsigned char *) buf, n, buf_len);
}

static int
tds_send_login(TDSSOCKET * tds, TDSCONNECTINFO * connect_info)
{
#ifdef WORDS_BIGENDIAN
	static const unsigned char be1[] = { 0x02, 0x00, 0x06, 0x04, 0x08, 0x01 };
#endif
	static const unsigned char le1[] = { 0x03, 0x01, 0x06, 0x0a, 0x09, 0x01 };
	static const unsigned char magic2[] = { 0x00, 0x00 };

	static const unsigned char magic3[] = { 0x00, 0x00, 0x00 };

	/* these seem to endian flags as well 13,17 on intel/alpha 12,16 on power */

#ifdef WORDS_BIGENDIAN
	static const unsigned char be2[] = { 0x00, 12, 16 };
#endif
	static const unsigned char le2[] = { 0x00, 13, 17 };

	/* 
	 * the former byte 0 of magic5 causes the language token and message to be 
	 * absent from the login acknowledgement if set to 1. There must be a way 
	 * of setting this in the client layer, but I am not aware of any thing of
	 * the sort -- bsb 01/17/99
	 */
	static const unsigned char magic5[] = { 0x00, 0x00 };
	static const unsigned char magic6[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	static const unsigned char magic7 = 0x01;

	static const unsigned char magic42[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	static const unsigned char magic50[] = { 0x00, 0x00, 0x00, 0x00 };

	/*
	 * capabilities are now part of the tds structure.
	 * unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x0A,104,0x00,0x00,0x00};
	 */
	/*
	 * This is the original capabilities packet we were working with (sqsh)
	 * unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x0A,104,0x00,0x00,0x00};
	 * original with 4.x messages
	 * unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x0D};
	 * This is isql 11.0.3
	 * unsigned char capabilities[]= {0x01,0x07,0x00,96, 129,207, 0xFF,0xFE,62,  0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x0D};
	 * like isql but with 5.0 messages
	 * unsigned char capabilities[]= {0x01,0x07,0x00,96, 129,207, 0xFF,0xFE,62,  0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x00};
	 */

	unsigned char protocol_version[4];
	unsigned char program_version[4];

	int len;
	char blockstr[16];

	if (IS_TDS42(tds)) {
		memcpy(protocol_version, "\004\002\000\000", 4);
		memcpy(program_version, "\004\002\000\000", 4);
	} else if (IS_TDS46(tds)) {
		memcpy(protocol_version, "\004\006\000\000", 4);
		memcpy(program_version, "\004\002\000\000", 4);
	} else if (IS_TDS50(tds)) {
		memcpy(protocol_version, "\005\000\000\000", 4);
		memcpy(program_version, "\005\000\000\000", 4);
	} else {
		tdsdump_log(TDS_DBG_SEVERE, "Unknown protocol version!\n");
		exit(1);
	}
	/*
	 * the following code is adapted from  Arno Pedusaar's 
	 * (psaar@fenar.ee) MS-SQL Client. His was a much better way to
	 * do this, (well...mine was a kludge actually) so here's mostly his
	 */

	tds_put_login_string(tds, connect_info->host_name, TDS_MAX_LOGIN_STR_SZ);	/* client host name */
	tds_put_login_string(tds, connect_info->user_name, TDS_MAX_LOGIN_STR_SZ);	/* account name */
	tds_put_login_string(tds, connect_info->password, TDS_MAX_LOGIN_STR_SZ);	/* account password */
	tds_put_login_string(tds, "37876", TDS_MAX_LOGIN_STR_SZ);	/* host process */
#ifdef WORDS_BIGENDIAN
	if (tds->emul_little_endian) {
		tds_put_n(tds, le1, 6);
	} else {
		tds_put_n(tds, be1, 6);
	}
#else
	tds_put_n(tds, le1, 6);
#endif
	tds_put_byte(tds, connect_info->bulk_copy);
	tds_put_n(tds, magic2, 2);
	if (IS_TDS42(tds)) {
		tds_put_int(tds, 512);
	} else {
		tds_put_int(tds, 0);
	}
	tds_put_n(tds, magic3, 3);
	tds_put_login_string(tds, connect_info->app_name, TDS_MAX_LOGIN_STR_SZ);
	tds_put_login_string(tds, connect_info->server_name, TDS_MAX_LOGIN_STR_SZ);
	if (IS_TDS42(tds)) {
		tds_put_login_string(tds, connect_info->password, 255);
	} else {
		len = strlen(connect_info->password);
		if (len > 253)
			len = 0;
		tds_put_byte(tds, 0);
		tds_put_byte(tds, len);
		tds_put_n(tds, connect_info->password, len);
		tds_put_n(tds, NULL, 253 - len);
		tds_put_byte(tds, len + 2);
	}

	tds_put_n(tds, protocol_version, 4);	/* TDS version; { 0x04,0x02,0x00,0x00 } */
	tds_put_login_string(tds, connect_info->library, 10);	/* client program name */
	if (IS_TDS42(tds)) {
		tds_put_int(tds, 0);
	} else {
		tds_put_n(tds, program_version, 4);	/* program version ? */
	}
#ifdef WORDS_BIGENDIAN
	if (tds->emul_little_endian) {
		tds_put_n(tds, le2, 3);
	} else {
		tds_put_n(tds, be2, 3);
	}
#else
	tds_put_n(tds, le2, 3);
#endif
	tds_put_login_string(tds, connect_info->language, TDS_MAX_LOGIN_STR_SZ);	/* language */
	tds_put_byte(tds, connect_info->suppress_language);
	tds_put_n(tds, magic5, 2);
	tds_put_byte(tds, connect_info->encrypted);
	tds_put_n(tds, magic6, 10);
	tds_put_login_string(tds, connect_info->char_set, TDS_MAX_LOGIN_STR_SZ);	/* charset */
	tds_put_byte(tds, magic7);

	/* network packet size */
	if (connect_info->block_size < 1000000)
		sprintf(blockstr, "%d", connect_info->block_size);
	else
		strcpy(blockstr, "512");
	tds_put_login_string(tds, blockstr, 6);

	if (IS_TDS42(tds)) {
		tds_put_n(tds, magic42, 8);
	} else if (IS_TDS46(tds)) {
		tds_put_n(tds, magic42, 4);
	} else if (IS_TDS50(tds)) {
		tds_put_n(tds, magic50, 4);
		tds_put_byte(tds, TDS_CAPABILITY_TOKEN);
		tds_put_smallint(tds, TDS_MAX_CAPABILITY);
		tds_put_n(tds, tds->capabilities, TDS_MAX_CAPABILITY);
	}

	return tds_flush_packet(tds);
}


int
tds7_send_auth(TDSSOCKET * tds, const unsigned char *challenge)
{
	int current_pos;
	TDSANSWER answer;

	/* FIXME: stuff duplicate in tds7_send_login */
	const char *domain;
	const char *user_name;
	const char *p;
	int user_name_len;
	int host_name_len;
	int password_len;
	int domain_len;

	TDSCONNECTINFO *connect_info = tds->connect_info;

	/* check connect_info */
	if (!connect_info)
		return TDS_FAIL;

	/* parse a bit of config */
	domain = connect_info->default_domain;
	user_name = connect_info->user_name;
	user_name_len = user_name ? strlen(user_name) : 0;
	host_name_len = connect_info->host_name ? strlen(connect_info->host_name) : 0;
	password_len = connect_info->password ? strlen(connect_info->password) : 0;
	domain_len = domain ? strlen(domain) : 0;

	/* check override of domain */
	if (user_name && (p = strchr(user_name, '\\')) != NULL) {
		domain = user_name;
		domain_len = p - user_name;

		user_name = p + 1;
		user_name_len = strlen(user_name);
	}

	tds->out_flag = 0x11;
	tds_put_n(tds, "NTLMSSP", 8);
	tds_put_int(tds, 3);	/* sequence 3 */

	current_pos = 64 + (domain_len + user_name_len + host_name_len) * 2;

	tds_put_smallint(tds, 24);	/* lan man resp length */
	tds_put_smallint(tds, 24);	/* lan man resp length */
	tds_put_int(tds, current_pos);	/* resp offset */
	current_pos += 24;

	tds_put_smallint(tds, 24);	/* nt resp length */
	tds_put_smallint(tds, 24);	/* nt resp length */
	tds_put_int(tds, current_pos);	/* nt resp offset */

	current_pos = 64;

	/* domain */
	tds_put_smallint(tds, domain_len * 2);
	tds_put_smallint(tds, domain_len * 2);
	tds_put_int(tds, current_pos);
	current_pos += domain_len * 2;

	/* username */
	tds_put_smallint(tds, user_name_len * 2);
	tds_put_smallint(tds, user_name_len * 2);
	tds_put_int(tds, current_pos);
	current_pos += user_name_len * 2;

	/* hostname */
	tds_put_smallint(tds, host_name_len * 2);
	tds_put_smallint(tds, host_name_len * 2);
	tds_put_int(tds, current_pos);
	current_pos += host_name_len * 2;

	/* unknown */
	tds_put_smallint(tds, 0);
	tds_put_smallint(tds, 0);
	tds_put_int(tds, current_pos + (24 * 2));

	/* flags */
	tds_put_int(tds, 0x8201);

	tds_put_string(tds, domain, domain_len);
	tds_put_string(tds, user_name, user_name_len);
	tds_put_string(tds, connect_info->host_name, host_name_len);

	tds_answer_challenge(connect_info->password, challenge, &answer);
	tds_put_n(tds, answer.lm_resp, 24);
	tds_put_n(tds, answer.nt_resp, 24);

	/* for security reason clear structure */
	memset(&answer, 0, sizeof(TDSANSWER));

	return tds_flush_packet(tds);
}

/**
 * tds7_send_login() -- Send a TDS 7.0 login packet
 * TDS 7.0 login packet is vastly different and so gets its own function
 */
static int
tds7_send_login(TDSSOCKET * tds, TDSCONNECTINFO * connect_info)
{
	int rc;
	static const unsigned char magic1_domain[] = { 6, 0x7d, 0x0f, 0xfd,
		0xff, 0x0, 0x0, 0x0,	/* Client PID */
		/* the 0x80 in the third byte controls whether this is a domain login 
		 * or not  0x80 = yes, 0x00 = no */
		0x0, 0xe0, 0x83, 0x0,	/* Connection ID of the Primary Server (?) */
		0x0,			/* Option Flags 1 */
		0x68,			/* Option Flags 2 */
		0x01,
		0x00,
		0x00, 0x09, 0x04, 0x00,
		0x00
	};
	static const unsigned char magic1_server[] = { 6, 0x83, 0xf2, 0xf8,	/* Client Program version */
		0xff, 0x0, 0x0, 0x0,	/* Client PID */
		0x0, 0xe0, 0x03, 0x0,	/* Connection ID of the Primary Server (?) */
		0x0,			/* Option Flags 1 */
		0x88,			/* Option Flags 2 */
		0xff,			/* Type Flags     */
		0xff,			/* reserved Flags */
		0xff, 0x36, 0x04, 0x00,
		0x00
	};
	unsigned const char *magic1 = magic1_server;
	unsigned char hwaddr[6];

	/* 0xb4,0x00,0x30,0x00,0xe4,0x00,0x00,0x00; */
	char unicode_string[256];
	int packet_size;
	int current_pos;
	static const unsigned char ntlm_id[] = "NTLMSSP";
	int domain_login = connect_info->try_domain_login ? 1 : 0;

	const char *domain = connect_info->default_domain;
	const char *user_name = connect_info->user_name;
	const char *p;
	int user_name_len = user_name ? strlen(user_name) : 0;
	int host_name_len = connect_info->host_name ? strlen(connect_info->host_name) : 0;
	int app_name_len = connect_info->app_name ? strlen(connect_info->app_name) : 0;
	int password_len = connect_info->password ? strlen(connect_info->password) : 0;
	int server_name_len = connect_info->server_name ? strlen(connect_info->server_name) : 0;
	int library_len = connect_info->library ? strlen(connect_info->library) : 0;
	int language_len = connect_info->language ? strlen(connect_info->language) : 0;
	int database_len = connect_info->database ? strlen(connect_info->database) : 0;
	int domain_len = domain ? strlen(domain) : 0;
	int auth_len = 0;

	/* avoid overflow limiting password */
	if (password_len > 128)
		password_len = 128;

	/* check override of domain */
	if (user_name && (p = strchr(user_name, '\\')) != NULL) {
		domain = user_name;
		domain_len = p - user_name;

		user_name = p + 1;
		user_name_len = strlen(user_name);

		domain_login = 1;
	}

	packet_size = 86 + (host_name_len + app_name_len + server_name_len + library_len + language_len + database_len) * 2;
	if (domain_login) {
		magic1 = magic1_domain;
		auth_len = 32 + host_name_len + domain_len;
		packet_size += auth_len;
	} else
		packet_size += (user_name_len + password_len) * 2;

	tds_put_smallint(tds, packet_size);
	tds_put_n(tds, NULL, 5);
	if (IS_TDS80(tds)) {
		tds_put_byte(tds, 0x80);
	} else {
		tds_put_byte(tds, 0x70);
	}
	tds_put_n(tds, NULL, 3);	/* rest of TDSVersion which is a 4 byte field    */
	tds_put_n(tds, NULL, 4);	/* desired packet size being requested by client */
	tds_put_n(tds, magic1, 21);

	current_pos = 86;	/* ? */
	/* host name */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, host_name_len);
	current_pos += host_name_len * 2;
	if (domain_login) {
		tds_put_smallint(tds, 0);
		tds_put_smallint(tds, 0);
		tds_put_smallint(tds, 0);
		tds_put_smallint(tds, 0);
	} else {
		/* username */
		tds_put_smallint(tds, current_pos);
		tds_put_smallint(tds, user_name_len);
		current_pos += user_name_len * 2;
		/* password */
		tds_put_smallint(tds, current_pos);
		tds_put_smallint(tds, password_len);
		current_pos += password_len * 2;
	}
	/* app name */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, app_name_len);
	current_pos += app_name_len * 2;
	/* server name */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, server_name_len);
	current_pos += server_name_len * 2;
	/* unknown */
	tds_put_smallint(tds, 0);
	tds_put_smallint(tds, 0);
	/* library name */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, library_len);
	current_pos += library_len * 2;
	/* language  - kostya@warmcat.excom.spb.su */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, language_len);
	current_pos += language_len * 2;
	/* database name */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, database_len);
	current_pos += database_len * 2;

	/* MAC address */
	tds_getmac(tds->s, hwaddr);
	tds_put_n(tds, hwaddr, 6);

	/* authentication stuff */
	tds_put_smallint(tds, current_pos);
	if (domain_login) {
		tds_put_smallint(tds, auth_len);	/* this matches numbers at end of packet */
		current_pos += auth_len;
	} else
		tds_put_smallint(tds, 0);

	/* unknown */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, 0);

	tds_put_string(tds, connect_info->host_name, host_name_len);
	if (!domain_login) {
		tds_put_string(tds, connect_info->user_name, user_name_len);
		tds7_ascii2unicode(tds, connect_info->password, unicode_string, 256);
		tds7_crypt_pass((unsigned char *) unicode_string, password_len * 2, (unsigned char *) unicode_string);
		tds_put_n(tds, unicode_string, password_len * 2);
	}
	tds_put_string(tds, connect_info->app_name, app_name_len);
	tds_put_string(tds, connect_info->server_name, server_name_len);
	tds_put_string(tds, connect_info->library, library_len);
	tds_put_string(tds, connect_info->language, language_len);
	tds_put_string(tds, connect_info->database, database_len);

	if (domain_login) {
		/* from here to the end of the packet is the NTLMSSP authentication */
		tds_put_n(tds, ntlm_id, 8);
		/* sequence 1 client -> server */
		tds_put_int(tds, 1);
		/* flags */
		tds_put_int(tds, 0xb201);

		/* domain info */
		tds_put_smallint(tds, domain_len);
		tds_put_smallint(tds, domain_len);
		tds_put_int(tds, 32 + host_name_len);

		/* hostname info */
		tds_put_smallint(tds, host_name_len);
		tds_put_smallint(tds, host_name_len);
		tds_put_int(tds, 32);

		/* hostname and domain */
		tds_put_n(tds, connect_info->host_name, host_name_len);
		tds_put_n(tds, domain, domain_len);
	}

	tdsdump_off();
	rc = tds_flush_packet(tds);
	tdsdump_on();

	return rc;
}

/**
 * tds7_crypt_pass() -- 'encrypt' TDS 7.0 style passwords.
 * the calling function is responsible for ensuring crypt_pass is at least 
 * 'len' characters
 */
unsigned char *
tds7_crypt_pass(const unsigned char *clear_pass, int len, unsigned char *crypt_pass)
{
	int i;
	unsigned char xormask = 0x5A;
	unsigned char hi_nibble, lo_nibble;

	for (i = 0; i < len; i++) {
		lo_nibble = (clear_pass[i] ^ xormask) >> 4;
		hi_nibble = (clear_pass[i] ^ xormask) << 4;
		crypt_pass[i] = hi_nibble | lo_nibble;
	}
	return crypt_pass;
}
