/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 * Copyright (C) 2005  Ziglio Frediano
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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <stdio.h>
#include <assert.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "tds.h"
#include "tdsiconv.h"
#include "tdsstring.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: login.c,v 1.138 2005-02-09 14:56:26 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int tds_send_login(TDSSOCKET * tds, TDSCONNECTION * connection);
static int tds8_do_login(TDSSOCKET * tds, TDSCONNECTION * connection);
static int tds7_send_login(TDSSOCKET * tds, TDSCONNECTION * connection);

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
tds_set_client_charset(TDSLOGIN * tds_login, const char *charset)
{
	tds_dstr_copy(&tds_login->client_charset, charset);
}

void
tds_set_language(TDSLOGIN * tds_login, const char *language)
{
	tds_dstr_copy(&tds_login->language, language);
}

void
tds_set_capabilities(TDSLOGIN * tds_login, unsigned char *capabilities, int size)
{
	memcpy(tds_login->capabilities, capabilities, size > TDS_MAX_CAPABILITY ? TDS_MAX_CAPABILITY : size);
}

/**
 * Do a connection to socket
 * @param tds connection structure. This should be a non-connected connection.
 * @param connection info for connection
 * @return TDS_FAIL or TDS_SUCCEED
 */
int
tds_connect(TDSSOCKET * tds, TDSCONNECTION * connection)
{
	int retval;
	int connect_timeout = 0;
	int db_selected = 0;
	char version[64];
	char *str;
	int len;

	/*
	 * If a dump file has been specified, start logging
	 */
	if (!tds_dstr_isempty(&connection->dump_file)) {
		if (connection->debug_flags)
			tds_debug_flags = connection->debug_flags;
		tdsdump_open(tds_dstr_cstr(&connection->dump_file));
	}

	tds->connection = connection;

	tds->major_version = connection->major_version;
	tds->minor_version = connection->minor_version;
	tds->emul_little_endian = connection->emul_little_endian;
#ifdef WORDS_BIGENDIAN
	if (IS_TDS7_PLUS(tds)) {
		/* TDS 7/8 only supports little endian */
		tds->emul_little_endian = 1;
	}
#endif

	/* set up iconv */
	if (connection->client_charset) {
		tds_iconv_open(tds, tds_dstr_cstr(&connection->client_charset));
	}

	/* specified a date format? */
	/*
	 * if (connection->date_fmt) {
	 * tds->date_fmt=strdup(connection->date_fmt);
	 * }
	 */
	connect_timeout = connection->connect_timeout;

	/* Jeff's hack - begin */
	tds->query_timeout = (connect_timeout) ? connection->query_timeout : 0;
	tds->query_timeout_func = connection->query_timeout_func;
	tds->query_timeout_param = connection->query_timeout_param;
	/* end */

	/* verify that ip_addr is not NULL */
	if (tds_dstr_isempty(&connection->ip_addr)) {
		tdsdump_log(TDS_DBG_ERROR, "IP address pointer is empty\n");
		if (connection->server_name) {
			tdsdump_log(TDS_DBG_ERROR, "Server %s not found!\n", tds_dstr_cstr(&connection->server_name));
		} else {
			tdsdump_log(TDS_DBG_ERROR, "No server specified!\n");
		}
		return TDS_FAIL;
	}

	if (!IS_TDS50(tds) && !tds_dstr_isempty(&connection->instance_name))
		connection->port = tds7_get_instance_port(tds_dstr_cstr(&connection->ip_addr), tds_dstr_cstr(&connection->instance_name));

	if (connection->port < 1) {
		tdsdump_log(TDS_DBG_ERROR, "invalid port number\n");
		return TDS_FAIL;
	}

	memcpy(tds->capabilities, connection->capabilities, TDS_MAX_CAPABILITY);

	retval = tds_version(tds, version);
	if (!retval)
		version[0] = '\0';

	if (tds_open_socket(tds, tds_dstr_cstr(&connection->ip_addr), connection->port, connect_timeout) != TDS_SUCCEED)
		return TDS_FAIL;

	if (IS_TDS80(tds)) {
		retval = tds8_do_login(tds, connection);
		db_selected = 1;
	} else if (IS_TDS7_PLUS(tds)) {
		retval = tds7_send_login(tds, connection);
		db_selected = 1;
	} else {
		tds->out_flag = 0x02;
		retval = tds_send_login(tds, connection);
	}
	if (retval == TDS_FAIL || !tds_process_login_tokens(tds)) {
		tds_close_socket(tds);
		tds_client_msg(tds->tds_ctx, tds, 20014, 9, 0, 0, "Login incorrect.");
		return TDS_FAIL;
	}

	if (connection->text_size || (!db_selected && !tds_dstr_isempty(&connection->database))) {
		len = 64 + tds_quote_id(tds, NULL, tds_dstr_cstr(&connection->database),-1);
		if ((str = (char *) malloc(len)) == NULL)
			return TDS_FAIL;

		str[0] = 0;
		if (connection->text_size) {
			sprintf(str, "set textsize %d ", connection->text_size);
		}
		if (!db_selected && !tds_dstr_isempty(&connection->database)) {
			strcat(str, "use ");
			tds_quote_id(tds, strchr(str, 0), tds_dstr_cstr(&connection->database), -1);
		}
		retval = tds_submit_query(tds, str);
		free(str);
		if (retval != TDS_SUCCEED)
			return TDS_FAIL;

		if (tds_process_simple_query(tds) != TDS_SUCCEED)
			return TDS_FAIL;
	}

	tds->connection = NULL;
	return TDS_SUCCEED;
}

static int
tds_put_login_string(TDSSOCKET * tds, const char *buf, int n)
{
	int buf_len = (buf ? strlen(buf) : 0);

	return tds_put_buf(tds, (const unsigned char *) buf, n, buf_len);
}

static int
tds_send_login(TDSSOCKET * tds, TDSCONNECTION * connection)
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
	const char *server_charset;

	int len;
	char blockstr[16];

	if (strchr(tds_dstr_cstr(&connection->user_name), '\\') != NULL) {
		tdsdump_log(TDS_DBG_ERROR, "NT login not support using TDS 4.x or 5.0\n");
		return TDS_FAIL;
	}

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

	tds_put_login_string(tds, tds_dstr_cstr(&connection->host_name), TDS_MAX_LOGIN_STR_SZ);	/* client host name */
	tds_put_login_string(tds, tds_dstr_cstr(&connection->user_name), TDS_MAX_LOGIN_STR_SZ);	/* account name */
	tds_put_login_string(tds, tds_dstr_cstr(&connection->password), TDS_MAX_LOGIN_STR_SZ);	/* account password */
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
	tds_put_byte(tds, connection->bulk_copy);
	tds_put_n(tds, magic2, 2);
	if (IS_TDS42(tds)) {
		tds_put_int(tds, 512);
	} else {
		tds_put_int(tds, 0);
	}
	tds_put_n(tds, magic3, 3);
	tds_put_login_string(tds, tds_dstr_cstr(&connection->app_name), TDS_MAX_LOGIN_STR_SZ);
	tds_put_login_string(tds, tds_dstr_cstr(&connection->server_name), TDS_MAX_LOGIN_STR_SZ);
	if (IS_TDS42(tds)) {
		tds_put_login_string(tds, tds_dstr_cstr(&connection->password), 255);
	} else {
		len = tds_dstr_len(&connection->password);
		if (len > 253)
			len = 0;
		tds_put_byte(tds, 0);
		tds_put_byte(tds, len);
		tds_put_n(tds, connection->password, len);
		tds_put_n(tds, NULL, 253 - len);
		tds_put_byte(tds, len + 2);
	}

	tds_put_n(tds, protocol_version, 4);	/* TDS version; { 0x04,0x02,0x00,0x00 } */
	tds_put_login_string(tds, tds_dstr_cstr(&connection->library), 10);	/* client program name */
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
	tds_put_login_string(tds, tds_dstr_cstr(&connection->language), TDS_MAX_LOGIN_STR_SZ);	/* language */
	tds_put_byte(tds, connection->suppress_language);
	tds_put_n(tds, magic5, 2);
	tds_put_byte(tds, connection->encrypted);
	tds_put_n(tds, magic6, 10);

	/* use charset nearest to client or nothing */
	server_charset = NULL;
	if (!tds_dstr_isempty(&connection->server_charset))
		server_charset = tds_dstr_cstr(&connection->server_charset);
	else
		server_charset = tds_sybase_charset_name(tds_dstr_cstr(&connection->client_charset));
	if (!server_charset)
		server_charset = "";
	tds_put_login_string(tds, server_charset, TDS_MAX_LOGIN_STR_SZ);	/* charset */
	/* this is a flag, mean that server should use character set provided by client */
	/* TODO notify charset change ?? what's correct meaning ?? -- freddy77 */
	tds_put_byte(tds, 1);

	/* network packet size */
	if (connection->block_size < 1000000 && connection->block_size)
		sprintf(blockstr, "%d", connection->block_size);
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

	TDSCONNECTION *connection = tds->connection;

	/* check connection */
	if (!connection)
		return TDS_FAIL;

	/* parse a bit of config */
	user_name = tds_dstr_cstr(&connection->user_name);
	user_name_len = user_name ? strlen(user_name) : 0;
	host_name_len = tds_dstr_len(&connection->host_name);
	password_len = tds_dstr_len(&connection->password);

	/* parse domain\username */
	if ((p = strchr(user_name, '\\')) == NULL)
		return TDS_FAIL;

	domain = user_name;
	domain_len = p - user_name;

	user_name = p + 1;
	user_name_len = strlen(user_name);

	tds->out_flag = 0x11;
	tds_put_n(tds, "NTLMSSP", 8);
	tds_put_int(tds, 3);	/* sequence 3 */

	/* FIXME *2 work only for single byte encodings */
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
	tds_put_string(tds, tds_dstr_cstr(&connection->host_name), host_name_len);

	tds_answer_challenge(tds_dstr_cstr(&connection->password), challenge, &answer);
	tds_put_n(tds, answer.lm_resp, 24);
	tds_put_n(tds, answer.nt_resp, 24);

	/* for security reason clear structure */
	memset(&answer, 0, sizeof(TDSANSWER));

	return tds_flush_packet(tds);
}

/**
 * tds7_send_login() -- Send a TDS 7.0 login packet
 * TDS 7.0 login packet is vastly different and so gets its own function
 * \returns the return value is ignored by the caller. :-/
 */
static int
tds7_send_login(TDSSOCKET * tds, TDSCONNECTION * connection)
{
	int rc;

	static const unsigned char client_progver[] = { 6, 0x83, 0xf2, 0xf8 };

	static const unsigned char tds7Version[] = { 0x00, 0x00, 0x00, 0x70 };
	static const unsigned char tds8Version[] = { 0x01, 0x00, 0x00, 0x71 };

	static const unsigned char connection_id[] = { 0x00, 0x00, 0x00, 0x00 };
	unsigned char option_flag1 = 0x00;
	unsigned char option_flag2 = tds->option_flag2;
	static const unsigned char sql_type_flag = 0x00;
	static const unsigned char reserved_flag = 0x00;

	static const unsigned char time_zone[] = { 0x88, 0xff, 0xff, 0xff };
	static const unsigned char collation[] = { 0x36, 0x04, 0x00, 0x00 };

	unsigned char hwaddr[6];

	/* 0xb4,0x00,0x30,0x00,0xe4,0x00,0x00,0x00; */
	char unicode_string[256];
	char *punicode;
	size_t unicode_left;
	int packet_size;
	int block_size;
	int current_pos;
	static const unsigned char ntlm_id[] = "NTLMSSP";
	int domain_login = 0;

	const char *domain = "";
	const char *user_name = tds_dstr_cstr(&connection->user_name);
	const char *p;
	int user_name_len = strlen(user_name);
	int host_name_len = tds_dstr_len(&connection->host_name);
	int app_name_len = tds_dstr_len(&connection->app_name);
	size_t password_len = tds_dstr_len(&connection->password);
	int server_name_len = tds_dstr_len(&connection->server_name);
	int library_len = tds_dstr_len(&connection->library);
	int language_len = tds_dstr_len(&connection->language);
	int database_len = tds_dstr_len(&connection->database);
	int domain_len = strlen(domain);
	int auth_len = 0;

	tds->out_flag = 0x10;

	/* avoid overflow limiting password */
	if (password_len > 128)
		password_len = 128;

	/* check override of domain */
	if ((p = strchr(user_name, '\\')) != NULL) {
		domain = user_name;
		domain_len = p - user_name;

		user_name = p + 1;
		user_name_len = strlen(user_name);

		domain_login = 1;
	}

	packet_size = 86 + (host_name_len + app_name_len + server_name_len + library_len + language_len + database_len) * 2;
	if (domain_login) {
		auth_len = 32 + host_name_len + domain_len;
		packet_size += auth_len;
	} else
		packet_size += (user_name_len + password_len) * 2;

	tds_put_int(tds, packet_size);
	if (IS_TDS80(tds)) {
		tds_put_n(tds, tds8Version, 4);
	} else {
		tds_put_n(tds, tds7Version, 4);
	}

	if (connection->block_size < 1000000)
		block_size = connection->block_size;
	else
		block_size = 4096;	/* SQL server default */
	tds_put_int(tds, block_size);	/* desired packet size being requested by client */

	tds_put_n(tds, client_progver, 4);	/* client program version ? */

	tds_put_int(tds, getpid());	/* process id of this process */

	tds_put_n(tds, connection_id, 4);

	option_flag1 |= 0x80;	/* enable warning messages if SET LANGUAGE issued   */
	option_flag1 |= 0x40;	/* change to initial database must succeed          */
	option_flag1 |= 0x20;	/* enable warning messages if USE <database> issued */

	tds_put_byte(tds, option_flag1);

	if (domain_login)
		option_flag2 |= 0x80;	/* enable domain login security                     */

	tds_put_byte(tds, option_flag2);

	tds_put_byte(tds, sql_type_flag);
	tds_put_byte(tds, reserved_flag);

	tds_put_n(tds, time_zone, 4);
	tds_put_n(tds, collation, 4);

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

	/* FIXME here we assume single byte, do not use *2 to compute bytes, convert before !!! */
	tds_put_string(tds, tds_dstr_cstr(&connection->host_name), host_name_len);
	if (!domain_login) {
		TDSICONV *char_conv = tds->char_convs[client2ucs2];
		tds_put_string(tds, tds_dstr_cstr(&connection->user_name), user_name_len);
		p = tds_dstr_cstr(&connection->password);
		punicode = unicode_string;
		unicode_left = sizeof(unicode_string);

		memset(&char_conv->suppress, 0, sizeof(char_conv->suppress));
		if (tds_iconv(tds, tds->char_convs[client2ucs2], to_server, &p, &password_len, &punicode, &unicode_left) ==
		    (size_t) - 1) {
			tdsdump_log(TDS_DBG_INFO1, "password \"%s\" could not be converted to UCS-2\n", p);
			assert(0);
		}
		password_len = punicode - unicode_string;
		tds7_crypt_pass((unsigned char *) unicode_string, password_len, (unsigned char *) unicode_string);
		tds_put_n(tds, unicode_string, password_len);
	}
	tds_put_string(tds, tds_dstr_cstr(&connection->app_name), app_name_len);
	tds_put_string(tds, tds_dstr_cstr(&connection->server_name), server_name_len);
	tds_put_string(tds, tds_dstr_cstr(&connection->library), library_len);
	tds_put_string(tds, tds_dstr_cstr(&connection->language), language_len);
	tds_put_string(tds, tds_dstr_cstr(&connection->database), database_len);

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
		tds_put_n(tds, connection->host_name, host_name_len);
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

#ifdef HAVE_GNUTLS

#include <gnutls/gnutls.h>

static ssize_t 
tds_pull_func(gnutls_transport_ptr ptr, void* data, size_t len)
{
	TDSSOCKET *tds = (TDSSOCKET *) ptr;
	int have;

	tdsdump_log(TDS_DBG_INFO1, "in tds_pull_func\n");
	
	/* if we have some data send it */
	if (tds->out_pos > 8)
		tds_flush_packet(tds);

	if (tds->tls_session) {
		/* read directly from socket */
		return tds_goodread(tds, data, len, 1);
	}

	for(;;) {
		have = tds->in_len - tds->in_pos;
		tdsdump_log(TDS_DBG_INFO1, "have %d\n", have);
		assert(have >= 0);
		if (have > 0)
			break;
		tdsdump_log(TDS_DBG_INFO1, "before read\n");
		if (tds_read_packet(tds) < 0)
			return -1;
		tdsdump_log(TDS_DBG_INFO1, "after read\n");
	}
	if (len > have)
		len = have;
	tdsdump_log(TDS_DBG_INFO1, "read %d bytes\n", len);
	memcpy(data, tds->in_buf + tds->in_pos, len);
	tds->in_pos += len;
	return len;
}

static ssize_t 
tds_push_func(gnutls_transport_ptr ptr, const void* data, size_t len)
{
	TDSSOCKET *tds = (TDSSOCKET *) ptr;
	tdsdump_log(TDS_DBG_INFO1, "in tds_push_func\n");
	
	if (tds->tls_session) {
		/* write to socket directly */
		return tds_goodwrite(tds, data, len, 1);
	}
	tds_put_n(tds, data, len);
	return len;
}


static void
tds_tls_log( int level, const char* s)
{
	tdsdump_log(TDS_DBG_INFO1, "GNUTLS: level %d:\n  %s", level, s);
}

static int tls_initialized = 0;

#ifdef TDS_ATTRIBUTE_DESTRUCTOR
static void __attribute__((destructor))
tds_tls_deinit(void)
{
	if (tls_initialized)
		gnutls_global_deinit();
}
#endif

#endif


static int
tds8_do_login(TDSSOCKET * tds, TDSCONNECTION * connection)
{
#ifdef HAVE_GNUTLS
	gnutls_session session;
	gnutls_certificate_credentials xcred;

	static const int kx_priority[] = {
		GNUTLS_KX_RSA_EXPORT, 
		GNUTLS_KX_RSA, GNUTLS_KX_DHE_DSS, GNUTLS_KX_DHE_RSA, 
		0
	};
	static const int cipher_priority[] = {
		GNUTLS_CIPHER_ARCFOUR_40,
		GNUTLS_CIPHER_DES_CBC,
/*	GNUTLS_CIPHER_AES_256_CBC, GNUTLS_CIPHER_AES_128_CBC,
	GNUTLS_CIPHER_3DES_CBC, GNUTLS_CIPHER_ARCFOUR_128,
*/
		0
	};
	static const int comp_priority[] = { GNUTLS_COMP_NULL, 0 };
	static const int mac_priority[] = {
		GNUTLS_MAC_SHA, GNUTLS_MAC_MD5, 0
	};
	int ret;
	const char *tls_msg;
#endif

	int i, len;
	const char *instance_name = tds_dstr_isempty(&connection->instance_name) ? "MSSQLServer" : tds_dstr_cstr(&connection->instance_name);
	int instance_name_len = strlen(instance_name) + 1;
	TDS_CHAR crypt_flag;
#define START_POS 21
#define UI16BE(n) ((n) >> 8), ((n) & 0xffu)
	TDS_UCHAR buf[] = {
		/* netlib version */
		0, UI16BE(START_POS), UI16BE(6),
		/* encryption */
		1, UI16BE(START_POS + 6), UI16BE(1),
		/* instance */
		2, UI16BE(START_POS + 6 + 1), UI16BE(instance_name_len),
		/* process id */
		3, UI16BE(START_POS + 6 + 1 + instance_name_len), UI16BE(4),
		/* end */
		0xff,
		/* netlib value */
		8, 0, 1, 0x55, 0, 0,
		/* encryption, normal */
		0
	};
	TDS_UCHAR *p;

	assert(sizeof(buf) == START_POS + 7);

	/* do prelogin */
	tds->out_flag = 0x12;

	tds_put_n(tds, buf, sizeof(buf));
	tds_put_n(tds, instance_name, instance_name_len);
	tds_put_int(tds, getpid());
	if (tds_flush_packet(tds) == TDS_FAIL)
		return TDS_FAIL;

	/* now process reply from server */
	len = tds_read_packet(tds);
	if (len <= 0 || tds->in_flag != 4)
		return TDS_FAIL;

	/* the only thing we care is flag */
	p = tds->in_buf;
	/* default 2, no certificate, no encryption */
	crypt_flag = 2;
	for (i = 0;; i += 5) {
		TDS_UCHAR type;
		int off, l;

		if (i >= len)
			return TDS_FAIL;
		type = p[i];
		if (type == 0xff)
			break;
		/* check packet */
		if (i+4 >= len)
			return TDS_FAIL;
		off = (((int) p[i+1]) << 8) | p[i+2];
		l = (((int) p[i+3]) << 8) | p[i+4];
		if (off > len || (off+l) > len)
			return TDS_FAIL;
		if (type == 1 && l >= 1) {
			crypt_flag = p[off];
		}
	}
	/* we readed all packet */
	tds->in_pos += len;
	/* TODO some mssql version do not set last packet, update tds according */

	/* no encryption required, just exit */
	tdsdump_log(TDS_DBG_INFO1, "detected flag %d\n", crypt_flag);
	if (crypt_flag == 2)
		return tds7_send_login(tds, connection);

#ifndef HAVE_GNUTLS
	return TDS_FAIL;
#else
	/* here we have to do encryption ... */

	xcred = NULL;
	session = NULL;	
	tls_msg = "initializing tls";

	/* FIXME place somewhere else, deinit at end */
	ret = gnutls_global_init();
	if (ret == 0) {
		tls_initialized = 1;

		gnutls_global_set_log_level(11);
		gnutls_global_set_log_function(tds_tls_log);
		tls_msg = "allocating credentials";
		ret = gnutls_certificate_allocate_credentials(&xcred);
	}

	if (ret == 0) {
		/* Initialize TLS session */
		tls_msg = "initializing session";
		ret = gnutls_init(&session, GNUTLS_CLIENT);
	}
	
	if (ret == 0) {
		gnutls_transport_set_ptr(session, tds);
		gnutls_transport_set_pull_function(session, tds_pull_func);
		gnutls_transport_set_push_function(session, tds_push_func);

		/* NOTE: there functions return int however they cannot fail */

		/* use default priorities... */
		gnutls_set_default_priority(session);

		/* ... but overwrite some */
		gnutls_cipher_set_priority(session, cipher_priority);
		gnutls_compression_set_priority(session, comp_priority);
		gnutls_kx_set_priority(session, kx_priority);
		gnutls_mac_set_priority(session, mac_priority);
		
		/* put the anonymous credentials to the current session */
		tls_msg = "setting credential";
		ret = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, xcred);
	}
	
	if (ret == 0) {
		/* Perform the TLS handshake */
		tls_msg = "handshake";
		ret = gnutls_handshake (session);
	}

	if (ret != 0) {
		if (session)
			gnutls_deinit(session);
		if (xcred)
			gnutls_certificate_free_credentials(xcred);
		tdsdump_log(TDS_DBG_ERROR, "%s failed: %s\n", tls_msg, gnutls_strerror (ret));
		return TDS_FAIL;
	}

	tdsdump_log(TDS_DBG_INFO1, "handshake succeeded!!\n");
	tds->tls_session = session;
	tds->tls_credentials = xcred;

	ret = tds7_send_login(tds, connection);

	/* if flag is 0 it means that after login server continue not encrypted */
	if (crypt_flag == 0 || ret != TDS_SUCCEED) {
		gnutls_deinit(tds->tls_session);
		tds->tls_session = NULL;
		gnutls_certificate_free_credentials(tds->tls_credentials);
		tds->tls_credentials = NULL;
	}
	
	return ret;
#endif
}
