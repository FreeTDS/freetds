/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2010  Frediano Ziglio
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

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_SSPI
#define SECURITY_WIN32

#include <windows.h>
#include <security.h>
#include <sspi.h>
#include <rpc.h>

#include "tds.h"
#include "tdsstring.h"
#include "replacements.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: sspi.c,v 1.2 2010-01-26 18:15:39 jklowden Exp $");

/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 * @{
 */

typedef struct tds_sspi_auth
{
	TDSAUTHENTICATION tds_auth;
	CredHandle cred;
	CtxtHandle cred_ctx;
} TDSSSPIAUTH;

static HMODULE secdll = NULL;
static PSecurityFunctionTableA sec_fn = NULL;

static int
tds_init_secdll(void)
{
	/* FIXME therad safe !!! */
	if (!secdll) {
		OSVERSIONINFO osver;

		memset(&osver, 0, sizeof(osver));
		osver.dwOSVersionInfoSize = sizeof(osver);
		if (!GetVersionEx(&osver))
			return 0;
		if (osver.dwPlatformId == VER_PLATFORM_WIN32_NT && osver.dwMajorVersion <= 4)
			secdll = LoadLibrary("security.dll");
		else
			secdll = LoadLibrary("secur32.dll");
		if (!secdll)
			return 0;
	}
	if (!sec_fn) {
		INIT_SECURITY_INTERFACE_A pInitSecurityInterface;

		pInitSecurityInterface = (INIT_SECURITY_INTERFACE_A) GetProcAddress(secdll, "InitSecurityInterfaceA");
		if (!pInitSecurityInterface)
			return 0;

		sec_fn = pInitSecurityInterface();
		if (!sec_fn)
			return 0;
	}
	return 1;
}

static int
tds_sspi_free(TDSSOCKET * tds, struct tds_authentication * tds_auth)
{
	TDSSSPIAUTH *auth = (TDSSSPIAUTH *) tds_auth;

	sec_fn->DeleteSecurityContext(&auth->cred_ctx);
	sec_fn->FreeCredentialsHandle(&auth->cred);
	free(auth->tds_auth.packet);
	free(auth);
	return TDS_SUCCEED;
}

enum { NTLMBUF_LEN = 4096 };

static int
tds_sspi_handle_next(TDSSOCKET * tds, struct tds_authentication * tds_auth, size_t len)
{
	SecBuffer in_buf, out_buf;
	SecBufferDesc in_desc, out_desc;
	SECURITY_STATUS status;
	ULONG attrs;
	TimeStamp ts;
	TDS_UCHAR *auth_buf;

	TDSSSPIAUTH *auth = (TDSSSPIAUTH *) tds_auth;

	if (len < 32 || len > NTLMBUF_LEN)
		return TDS_FAIL;

	auth_buf = (TDS_UCHAR *) malloc(len);
	if (!auth_buf)
		return TDS_FAIL;
	tds_get_n(tds, auth_buf, (int)len);

	in_desc.ulVersion  = out_desc.ulVersion  = SECBUFFER_VERSION;
	in_desc.cBuffers   = out_desc.cBuffers   = 1;
	in_desc.pBuffers   = &in_buf;
	out_desc.pBuffers   = &out_buf;

	in_buf.BufferType = SECBUFFER_TOKEN;
	in_buf.pvBuffer   = auth_buf;
	in_buf.cbBuffer   = (ULONG)len;

	out_buf.BufferType = SECBUFFER_TOKEN;
	out_buf.pvBuffer   = auth->tds_auth.packet;
	out_buf.cbBuffer   = NTLMBUF_LEN;

	status = sec_fn->InitializeSecurityContextA(&auth->cred, &auth->cred_ctx, NULL,  
		ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONNECTION,
		0, SECURITY_NETWORK_DREP, &in_desc,
		0, &auth->cred_ctx, &out_desc,
		&attrs, &ts);

	free(auth_buf);

	if (status != SEC_E_OK)
		return TDS_FAIL;

	tds_put_n(tds, auth->tds_auth.packet, out_buf.cbBuffer);

	return tds_flush_packet(tds);
}

/**
 * Build a SSPI packet to send to server
 * @param tds     A pointer to the TDSSOCKET structure managing a client/server operation.
 */
TDSAUTHENTICATION *
tds_sspi_get_auth(TDSSOCKET * tds)
{
	SecBuffer buf;
	SecBufferDesc desc;
	SECURITY_STATUS status;
	ULONG attrs;
	TimeStamp ts;

	TDSSSPIAUTH *auth;

	if (!tds_init_secdll())
		return NULL;

	auth = (TDSSSPIAUTH *) calloc(1, sizeof(TDSSSPIAUTH));
	if (!auth || !tds->connection)
		return NULL;

	auth->tds_auth.free = tds_sspi_free;
	auth->tds_auth.handle_next = tds_sspi_handle_next;

	/* TODO parse user/pass and use them if available */
	/* TODO use SPNEGO to use Kerberos if possible */
	if (sec_fn->AcquireCredentialsHandleA(NULL, (char *)"NTLM", SECPKG_CRED_OUTBOUND,
		NULL, NULL, // ntlm->p_identity,
		NULL, NULL, &auth->cred, &ts) != SEC_E_OK) {
		free(auth);
		return NULL;
	}

	auth->tds_auth.packet = (TDS_UCHAR *) malloc(NTLMBUF_LEN);
	if (!auth->tds_auth.packet) {
		sec_fn->FreeCredentialsHandle(&auth->cred);
		free(auth);
		return NULL;
	}
	desc.ulVersion = SECBUFFER_VERSION;
	desc.cBuffers  = 1;
	desc.pBuffers  = &buf;

	buf.cbBuffer   = NTLMBUF_LEN;
	buf.BufferType = SECBUFFER_TOKEN;
	buf.pvBuffer   = auth->tds_auth.packet;

	status = sec_fn->InitializeSecurityContext(&auth->cred, NULL, "",
		ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONNECTION,
		0, SECURITY_NETWORK_DREP,
		NULL, 0,
		&auth->cred_ctx, &desc,
		&attrs, &ts);

	if (status == SEC_I_COMPLETE_AND_CONTINUE || status == SEC_I_CONTINUE_NEEDED) {
		sec_fn->CompleteAuthToken(&auth->cred_ctx, &desc);
	} else if(status != SEC_E_OK) {
		free(auth->tds_auth.packet);
		sec_fn->FreeCredentialsHandle(&auth->cred);
		free(auth);
		return NULL;
	}

	auth->tds_auth.packet_len = buf.cbBuffer;
	return &auth->tds_auth;
}

#endif /* HAVE_SSPI */

/** @} */

