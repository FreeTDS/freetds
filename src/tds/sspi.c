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
#undef UNICODE
#undef _UNICODE
#define UNICODE 1
#define _UNICODE 1

/* enabled some additional definitions for getaddrinfo */
#define _WIN32_WINNT 0x601

#include <config.h>

/* fix possible bug in sspi.h header */
#define FreeCredentialHandle FreeCredentialsHandle

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_SSPI
#define SECURITY_WIN32

#include <freetds/windows.h>
#include <security.h>
#include <sspi.h>
#include <rpc.h>
#include <tchar.h>

#include <freetds/tds.h>
#include <freetds/thread.h>
#include <freetds/utils/string.h>
#include <freetds/bool.h>
#include <freetds/replacements.h>
#include <freetds/iconv.h>

/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 * @{
 */

#define PRIsSTR "S"

typedef struct tds_sspi_auth
{
	TDSAUTHENTICATION tds_auth;
	CredHandle cred;
	CtxtHandle cred_ctx;
	TCHAR *sname;
} TDSSSPIAUTH;

static HMODULE secdll = NULL;
static PSecurityFunctionTable sec_fn = NULL;
static tds_mutex sec_mutex = TDS_MUTEX_INITIALIZER;

static bool
tds_init_secdll(void)
{
	bool res = false;

	if (sec_fn)
		return true;

	tds_mutex_lock(&sec_mutex);
	for (;;) {
		if (!secdll) {
			secdll = LoadLibrary(TEXT("secur32.dll"));
			if (!secdll)
				break;
		}
		if (!sec_fn) {
			INIT_SECURITY_INTERFACE pInitSecurityInterface;

			pInitSecurityInterface = (INIT_SECURITY_INTERFACE_W) GetProcAddress(secdll, "InitSecurityInterfaceW");
			if (!pInitSecurityInterface)
				break;

			sec_fn = pInitSecurityInterface();
			if (!sec_fn)
				break;
		}
		res = true;
		break;
	}
	tds_mutex_unlock(&sec_mutex);
	return res;
}

static int
tds_sspi_free(TDSCONNECTION * conn TDS_UNUSED, struct tds_authentication * tds_auth)
{
	TDSSSPIAUTH *auth = (TDSSSPIAUTH *) tds_auth;

	if (SecIsValidHandle(&auth->cred_ctx))
		sec_fn->DeleteSecurityContext(&auth->cred_ctx);
	if (SecIsValidHandle(&auth->cred))
		sec_fn->FreeCredentialsHandle(&auth->cred);
	if (auth->tds_auth.packet)
		sec_fn->FreeContextBuffer(auth->tds_auth.packet);
	free(auth->sname);
	free(auth);
	return TDS_SUCCESS;
}

static int
tds_sspi_handle_next(TDSSOCKET * tds, struct tds_authentication * tds_auth, size_t len)
{
	SecBuffer in_buf, out_buf;
	SecBufferDesc in_desc, out_desc;
	SECURITY_STATUS status;
	ULONG attrs;
	TimeStamp ts;
	uint8_t *auth_buf;

	TDSSSPIAUTH *auth = (TDSSSPIAUTH *) tds_auth;

	if (len < 1)
		return TDS_FAIL;

	auth_buf = tds_new(uint8_t, len);
	if (!auth_buf)
		return TDS_FAIL;
	tds_get_n(tds, auth_buf, (int)len);

	/* free previously allocated buffer */
	if (auth->tds_auth.packet) {
		sec_fn->FreeContextBuffer(auth->tds_auth.packet);
		auth->tds_auth.packet = NULL;
	}
	in_desc.ulVersion  = out_desc.ulVersion  = SECBUFFER_VERSION;
	in_desc.cBuffers   = out_desc.cBuffers   = 1;
	in_desc.pBuffers   = &in_buf;
	out_desc.pBuffers   = &out_buf;

	in_buf.BufferType = SECBUFFER_TOKEN;
	in_buf.pvBuffer   = auth_buf;
	in_buf.cbBuffer   = (ULONG)len;

	out_buf.BufferType = SECBUFFER_TOKEN;
	out_buf.pvBuffer   = NULL;
	out_buf.cbBuffer   = 0;

	status = sec_fn->InitializeSecurityContext(&auth->cred, &auth->cred_ctx, auth->sname,
		ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONNECTION | ISC_REQ_ALLOCATE_MEMORY,
		0, SECURITY_NETWORK_DREP, &in_desc,
		0, &auth->cred_ctx, &out_desc,
		&attrs, &ts);

	free(auth_buf);
	auth->tds_auth.packet = out_buf.pvBuffer;

	switch (status) {
	case SEC_I_COMPLETE_AND_CONTINUE:
		sec_fn->CompleteAuthToken(&auth->cred_ctx, &out_desc);
		break;

	case SEC_I_CONTINUE_NEEDED:
	case SEC_E_OK:
		break;

	default:
		return TDS_FAIL;
	}

	if (out_buf.cbBuffer == 0)
		return TDS_SUCCESS;

	tds_put_n(tds, auth->tds_auth.packet, out_buf.cbBuffer);

	return tds_flush_packet(tds);
}

static size_t
convert_to_ucs2le_string(TDSSOCKET * tds, const char *s, size_t len, WCHAR *out, size_t out_len)
{
	const char *ib;
	char *ob;
	size_t il, ol;

	TDSICONV * char_conv = tds->conn->char_convs[client2ucs2];

	/* char_conv is only mostly const */
	TDS_ERRNO_MESSAGE_FLAGS *suppress = (TDS_ERRNO_MESSAGE_FLAGS *) & char_conv->suppress;

	/* convert */
	ib = s;
	il = len;
	ob = (char *) out;
	ol = out_len * 2;
	memset(suppress, 0, sizeof(char_conv->suppress));
	if (tds_iconv(tds, char_conv, to_server, &ib, &il, &ob, &ol) == (size_t) - 1)
		return (size_t) -1;

	return ob - (char *) out;
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
	SEC_WINNT_AUTH_IDENTITY identity;
	const char *server_name;
	WCHAR *p, *user_name, *passwd;
	struct addrinfo *addrs = NULL;
	static WCHAR sec_package[] = L"Negotiate";
	WCHAR user_ucs2[512], password_ucs2[256];

	TDSSSPIAUTH *auth;
	TDSLOGIN *login = tds->login;

	/* check login */
	if (!login)
		return NULL;

	if (!tds_init_secdll())
		return NULL;

	/* parse username/password information */
	memset(&identity, 0, sizeof(identity));
	if (convert_to_ucs2le_string(tds, tds_dstr_cstr(&login->user_name), tds_dstr_len(&login->user_name)+1,
				     user_ucs2, TDS_VECTOR_SIZE(user_ucs2)) == (size_t) -1)
		return NULL;
	user_name = user_ucs2;
	if (convert_to_ucs2le_string(tds, tds_dstr_cstr(&login->password), tds_dstr_len(&login->password)+1,
				     password_ucs2, TDS_VECTOR_SIZE(password_ucs2)) == (size_t) -1)
		return NULL;
	passwd = password_ucs2;
	if ((p = wcschr(user_name, L'\\')) != NULL) {
		identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
		identity.Password = passwd;
		identity.PasswordLength = (unsigned long) wcslen(passwd);
		identity.Domain = user_name;
		identity.DomainLength = (unsigned long) (p - user_name);
		user_name = p + 1;
		identity.User = user_name;
		identity.UserLength = (unsigned long) wcslen(user_name);
	}

	auth = tds_new0(TDSSSPIAUTH, 1);
	if (!auth)
		return NULL;

	auth->tds_auth.free = tds_sspi_free;
	auth->tds_auth.handle_next = tds_sspi_handle_next;
	SecInvalidateHandle(&auth->cred);
	SecInvalidateHandle(&auth->cred_ctx);

	/* using Negotiate system will use proper protocol (either NTLM or Kerberos) */
	if (sec_fn->AcquireCredentialsHandle(NULL, sec_package, SECPKG_CRED_OUTBOUND,
		NULL, identity.Domain ? &identity : NULL,
		NULL, NULL, &auth->cred, &ts) != SEC_E_OK) {
		free(auth);
		return NULL;
	}

	desc.ulVersion = SECBUFFER_VERSION;
	desc.cBuffers  = 1;
	desc.pBuffers  = &buf;

	buf.BufferType = SECBUFFER_TOKEN;
	buf.pvBuffer   = NULL;
	buf.cbBuffer   = 0;

	/* build SPN */
	server_name = tds_dstr_cstr(&login->server_host_name);
	if (strchr(server_name, '.') == NULL) {
		struct addrinfo hints;
		int res;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_V4MAPPED|AI_ADDRCONFIG|AI_CANONNAME|AI_FQDN;
		res = getaddrinfo(server_name, NULL, &hints, &addrs);
		if (res) {
			/* some version of Windows does not support V4MAPPED
			 * and ADDRCONFIG, try without them
			 */
			hints.ai_flags = AI_CANONNAME|AI_FQDN;
			res = getaddrinfo(server_name, NULL, &hints, &addrs);
		}
		if (!res && addrs->ai_canonname && strchr(addrs->ai_canonname, '.') != NULL)
			server_name = addrs->ai_canonname;
	}
	if (strchr(server_name, '.') != NULL) {
		auth->sname = malloc((strlen(server_name) + 24) * sizeof(TCHAR));
		if (!auth->sname) {
			if (addrs)
				freeaddrinfo(addrs);
			tds_sspi_free(tds->conn, &auth->tds_auth);
			return NULL;
		}
		wsprintf(auth->sname, TEXT("MSSQLSvc/%") TEXT(PRIsSTR) TEXT(":%d"), server_name, login->port);
		tdsdump_log(TDS_DBG_NETWORK, "kerberos name %" PRIsSTR "\n", auth->sname);
	}
	if (addrs)
		freeaddrinfo(addrs);

	status = sec_fn->InitializeSecurityContext(&auth->cred, NULL, auth->sname,
		ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONNECTION | ISC_REQ_ALLOCATE_MEMORY,
		0, SECURITY_NETWORK_DREP,
		NULL, 0,
		&auth->cred_ctx, &desc,
		&attrs, &ts);

	switch (status) {
	case SEC_I_COMPLETE_AND_CONTINUE:
		sec_fn->CompleteAuthToken(&auth->cred_ctx, &desc);
		break;

	case SEC_I_CONTINUE_NEEDED:
	case SEC_E_OK:
		break;

	default:
		tds_sspi_free(tds->conn, &auth->tds_auth);
		return NULL;
	}

	auth->tds_auth.packet_len = buf.cbBuffer;
	auth->tds_auth.packet     = buf.pvBuffer;

	return &auth->tds_auth;
}

/** @} */

#endif /* HAVE_SSPI */
