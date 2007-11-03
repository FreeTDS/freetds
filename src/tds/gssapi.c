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

#ifdef ENABLE_DEVELOPING

#include <gssapi/gssapi_generic.h>
#include <gssapi/gssapi_krb5.h>

#include "tds.h"
#include "tdsstring.h"
#include "replacements.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: gssapi.c,v 1.5 2007-11-03 13:36:40 freddy77 Exp $");

/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 * @{ 
 */

/**
 * Build a GSSAPI packet to send to server
 * @param tds     A pointer to the TDSSOCKET structure managing a client/server operation.
 * @param packet  GSSAPI packet build from function
 * @return size of packet
 */
int
tds_get_gss_packet(TDSSOCKET * tds, TDS_UCHAR ** gss_packet)
{
	/*
	 * TODO
	 * There are some differences between this implementation and MS on
	 * - MS use SPNEGO with 3 mechnisms (MS KRB5, KRB5, NTLMSSP)
	 * - MS seems to use MUTUAL flag
	 * - name type is "Service and Instance (2)" and not "Principal (1)"
	 * remove memory leaks !!!
	 * check for errors in many functions
	 * a bit more verbose
	 * dinamically load library ??
	 */
	gss_ctx_id_t context;
	gss_ctx_id_t *gss_context = &context;
	gss_buffer_desc send_tok, *token_ptr;
/*	gss_buffer_desc recv_tok; */
	gss_name_t target_name;
	OM_uint32 maj_stat, min_stat;
	char *sname = NULL;
	/* same as GSS_KRB5_NT_PRINCIPAL_NAME but do not require .so library */
	static const gss_OID_desc nt_principal = { 10, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01" };
	OM_uint32 ret_flags;
	int ret_len;

	if (!tds->connection)
		return -1;

	if (asprintf(&sname, "MSSQLSvc/%s:%d", tds_dstr_cstr(&tds->connection->server_host_name), tds->connection->port) < 0)
		return -1;
	tdsdump_log(TDS_DBG_NETWORK, "kerberos name %s\n", sname);

	/*
	 * Import the name into target_name.  Use send_tok to save
	 * local variable space.
	 */
	send_tok.value = sname;
	send_tok.length = strlen(sname);
	maj_stat = gss_import_name(&min_stat, &send_tok, &nt_principal, &target_name);
	if (maj_stat != GSS_S_COMPLETE)
		return -1;

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
	*gss_context = GSS_C_NO_CONTEXT;

	do {
		maj_stat = gss_init_sec_context(&min_stat, GSS_C_NO_CREDENTIAL, gss_context, target_name, 
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
/*		(void) gss_release_buffer(&min_stat, &send_tok); */

		if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED)
			break;

/*
		if (maj_stat == GSS_S_CONTINUE_NEEDED) {
			if (recv_token(s, &token_flags, &recv_tok) < 0) {
				(void) gss_release_name(&min_stat, &target_name);
				return -1;
			}
			token_ptr = &recv_tok;
		}
*/

		(void) gss_release_name(&min_stat, &target_name);
		free(sname);
		if (*gss_context != GSS_C_NO_CONTEXT)
			gss_delete_sec_context(&min_stat, gss_context, GSS_C_NO_BUFFER);

		*gss_packet = (TDS_UCHAR *) malloc(send_tok.length);
		ret_len = send_tok.length;
		memcpy(*gss_packet, send_tok.value, send_tok.length);
		gss_release_buffer(&min_stat, &send_tok);
//		*gss_packet = send_tok.value;
//		return send_tok.length;
		return ret_len;

	} while (maj_stat == GSS_S_CONTINUE_NEEDED);

	(void) gss_release_name(&min_stat, &target_name);
	free(sname);
	if (*gss_context != GSS_C_NO_CONTEXT)
		gss_delete_sec_context(&min_stat, gss_context, GSS_C_NO_BUFFER);

	return -1;
}

#endif

/** @} */

