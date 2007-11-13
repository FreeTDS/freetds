/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2007  Frediano Ziglio
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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <ctype.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */

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

#ifdef ENABLE_KRB5

#include <gssapi/gssapi_generic.h>
#include <gssapi/gssapi_krb5.h>

#include "tds.h"
#include "tdsstring.h"
#include "replacements.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: gssapi.c,v 1.10 2007-11-13 09:14:57 freddy77 Exp $");

/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 * @{ 
 */

typedef struct tds_gss_auth
{
	TDSAUTHENTICATION tds_auth;
	gss_ctx_id_t gss_context;
	gss_name_t target_name;
	char *sname;
} TDSGSSAUTH;

static int
tds_gss_free(TDSSOCKET * tds, struct tds_authentication * tds_auth)
{
	TDSGSSAUTH *auth = (TDSGSSAUTH *) tds_auth;
	OM_uint32 min_stat;

	if (auth->tds_auth.packet) {
		gss_buffer_desc send_tok;

		send_tok.value = (void *) auth->tds_auth.packet;
		send_tok.length = auth->tds_auth.packet_len;
		gss_release_buffer(&min_stat, &send_tok);
	}

	gss_release_name(&min_stat, &auth->target_name);
	free(auth->sname);
	if (auth->gss_context != GSS_C_NO_CONTEXT)
		gss_delete_sec_context(&min_stat, &auth->gss_context, GSS_C_NO_BUFFER);
	free(auth);

	return TDS_SUCCEED;
}

static int
tds_gss_handle_next(TDSSOCKET * tds, struct tds_authentication * auth, size_t len)
{
	return TDS_FAIL;
}

/**
 * Build a GSSAPI packet to send to server
 * @param tds     A pointer to the TDSSOCKET structure managing a client/server operation.
 * @param packet  GSSAPI packet build from function
 * @return size of packet
 */
TDSAUTHENTICATION * 
tds_gss_get_auth(TDSSOCKET * tds)
{
	/*
	 * TODO
	 * There are some differences between this implementation and MS on
	 * - MS use SPNEGO with 3 mechnisms (MS KRB5, KRB5, NTLMSSP)
	 * - MS seems to use MUTUAL flag
	 * - name type is "Service and Instance (2)" and not "Principal (1)"
	 * check for memory leaks
	 * check for errors in many functions
	 * a bit more verbose
	 * dinamically load library ??
	 */
	gss_buffer_desc send_tok, *token_ptr;
	OM_uint32 maj_stat, min_stat;
	/* same as GSS_KRB5_NT_PRINCIPAL_NAME but do not require .so library */
	static gss_OID_desc nt_principal = { 10, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01" };
	OM_uint32 ret_flags;
	const char *server_name;
	/* Storage for reentrant getaddrby* calls */
	char buffer[4096];

	struct tds_gss_auth *auth = (struct tds_gss_auth *) calloc(1, sizeof(struct tds_gss_auth));

	if (!auth || !tds->connection)
		return NULL;

	auth->tds_auth.free = tds_gss_free;
	auth->tds_auth.handle_next = tds_gss_handle_next;

	server_name = tds_dstr_cstr(&tds->connection->server_host_name);
	if (strchr(server_name, '.') == NULL) {
		struct hostent result;
		int h_errnop;

		struct hostent *host = tds_gethostbyname_r(server_name, &result, buffer, sizeof(buffer), &h_errnop);
		if (host && strchr(host->h_name, '.') != NULL)
			server_name = host->h_name;
	}

	if (asprintf(&auth->sname, "MSSQLSvc/%s:%d", server_name, tds->connection->port) < 0) {
		tds_gss_free(tds, (TDSAUTHENTICATION *) auth);
		return NULL;
	}
	tdsdump_log(TDS_DBG_NETWORK, "kerberos name %s\n", auth->sname);

	/*
	 * Import the name into target_name.  Use send_tok to save
	 * local variable space.
	 */
	send_tok.value = auth->sname;
	send_tok.length = strlen(auth->sname);
	maj_stat = gss_import_name(&min_stat, &send_tok, &nt_principal, &auth->target_name);
	if (maj_stat != GSS_S_COMPLETE) {
		tds_gss_free(tds, (TDSAUTHENTICATION *) auth);
		return NULL;
	}

	send_tok.value = NULL;
	send_tok.length = 0;

	/*
	 * Perform the context-establishement loop.
	 *
	 * On each pass through the loop, token_ptr points to the token
	 * to send to the server (or GSS_C_NO_BUFFER on the first pass).
	 * Every generated token is stored in send_tok which is then
	 * transmitted to the server; every received token is stored in
	 * recv_tok, which token_ptr is then set to, to be processed by
	 * the next call to gss_init_sec_context.
	 * 
	 * GSS-API guarantees that send_tok's length will be non-zero
	 * if and only if the server is expecting another token from us,
	 * and that gss_init_sec_context returns GSS_S_CONTINUE_NEEDED if
	 * and only if the server has another token to send us.
	 */

	token_ptr = GSS_C_NO_BUFFER;
	auth->gss_context = GSS_C_NO_CONTEXT;

	maj_stat = gss_init_sec_context(&min_stat, GSS_C_NO_CREDENTIAL, &auth->gss_context, auth->target_name, 
					/* GSS_C_DELEG_FLAG GSS_C_MUTUAL_FLAG ?? */
					GSS_C_NULL_OID, GSS_C_REPLAY_FLAG, 0, NULL,	/* no channel bindings */
					token_ptr, NULL,	/* ignore mech type */
					&send_tok, &ret_flags, NULL);	/* ignore time_rec */

/*
	if (token_ptr != GSS_C_NO_BUFFER)
		free(recv_tok.value);
*/

/*
	if (send_tok.length != 0) {
		if (send_token(s, v1_format ? 0 : TOKEN_CONTEXT, &send_tok) < 0) {
			(void) gss_release_buffer(&min_stat, &send_tok);
			(void) gss_release_name(&min_stat, &target_name);
			return -1;
		}
*/
/*	(void) gss_release_buffer(&min_stat, &send_tok); */

	if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED) {
		gss_release_buffer(&min_stat, &send_tok);
		tds_gss_free(tds, (TDSAUTHENTICATION *) auth);
		return NULL;
	}

/*
	if (maj_stat == GSS_S_CONTINUE_NEEDED) {
		if (recv_token(s, &token_flags, &recv_tok) < 0) {
			(void) gss_release_name(&min_stat, &target_name);
			return -1;
		}
		token_ptr = &recv_tok;
	}
*/

	auth->tds_auth.packet = (TDS_UCHAR *) send_tok.value;
	auth->tds_auth.packet_len = send_tok.length;
	return (TDSAUTHENTICATION *) auth;
}

#endif

/** @} */

