/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2026  Frediano Ziglio
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

/*
 * Purpose: test NTLMv2 code (session key, key exchange, MIC and client blob).
 * Values are from MS-NLMP section 4.2.4 (NTLMv2 authentication).
 */
#undef NDEBUG
#include "../challenge.c"

#include "common.h"
#include <assert.h>

static char *
bin2ascii(char *dest, const void *data, size_t len)
{
	char *s = dest;
	const unsigned char *src = (const unsigned char *) data;

	for (; len > 0; --len, s += 2)
		sprintf(s, "%02x", *src++);
	*s = 0;
	return dest;
}

static void
check(const char *name, const void *data, size_t len, const char *expected)
{
	char buf[1024];

	assert(len * 2 < sizeof(buf));
	bin2ascii(buf, data, len);
	if (strcmp(buf, expected) != 0) {
		fprintf(stderr, "Wrong %s: got %s expected %s\n", name, buf, expected);
		exit(1);
	}
}

/* NTOWFv2("Password", "User", "Domain") */
static const unsigned char ntowfv2[16] = {
	0x0c, 0x86, 0x8a, 0x40, 0x3b, 0xfd, 0x7a, 0x93, 0xa3, 0x00, 0x1e, 0xf2, 0x2e, 0xf0, 0x2e, 0x3f
};

static const unsigned char server_challenge[8] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef };

static const unsigned char client_challenge[8] = { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa };

/* MsvAvNbDomainName "Domain", MsvAvNbComputerName "Server", MsvAvEOL */
static const unsigned char target_info[] = {
	0x02, 0x00, 0x0c, 0x00, 'D', 0, 'o', 0, 'm', 0, 'a', 0, 'i', 0, 'n', 0,
	0x01, 0x00, 0x0c, 0x00, 'S', 0, 'e', 0, 'r', 0, 'v', 0, 'e', 0, 'r', 0,
	0x00, 0x00, 0x00, 0x00
};

#define TARGET_INFO_PAIRS_LEN (sizeof(target_info) - 4)
#define PREFIX_LEN TDS_OFFSET(names_blob_prefix_t, target_info)

static void
test_responses(void)
{
	unsigned char temp[PREFIX_LEN + sizeof(target_info) + 4];
	unsigned char *response;
	unsigned char session_base_key[16], encrypted_key[16], random_key[16];

	/* LMv2 response */
	response = make_lm_v2_response(ntowfv2, client_challenge, 8, server_challenge);
	assert(response);
	check("LMv2 response", response, 16, "86c35097ac9cec102554764a57cccc19");
	free(response);

	/* temp of the example: version, Z(6), time 0, client challenge, Z(4), target info, Z(4) */
	memset(temp, 0, sizeof(temp));
	temp[0] = temp[1] = 1;
	memcpy(temp + 16, client_challenge, 8);
	memcpy(temp + PREFIX_LEN, target_info, sizeof(target_info));

	/* NTProofStr is the first part of NTLMv2 response */
	response = make_lm_v2_response(ntowfv2, temp, sizeof(temp), server_challenge);
	assert(response);
	check("NTProofStr", response, 16, "68cd0ab851e51c96aabc927bebef6a1c");

	/* SessionBaseKey, computed like tds_answer_challenge_ntlmv2 */
	hmac_md5(ntowfv2, response, 16, session_base_key);
	check("SessionBaseKey", session_base_key, 16, "8de40ccadbc14a82f15cb0ad0de95ca3");
	free(response);

	/* EncryptedRandomSessionKey */
	memset(random_key, 0x55, sizeof(random_key));
	tds_rc4(session_base_key, 16, random_key, 16, encrypted_key);
	check("EncryptedRandomSessionKey", encrypted_key, 16, "c5dad2544fc9799094ce1ce90bc9d03e");
}

static void
test_mic(void)
{
	unsigned char key[16], mic[16];

	memset(key, 0x11, sizeof(key));
	assert(tds_ntlm_mic(key, (const unsigned char *) "NEGOTIATE", 9, (const unsigned char *) "CHALLENGE", 9,
			    (const unsigned char *) "AUTHENTICATE", 12, mic));
	check("MIC", mic, 16, "762ceb6c2e0cd518d0de6c88e1c57f3f");
}

static void
test_names_blob(void)
{
	TDSNTLMAUTH auth;
	unsigned char info[sizeof(target_info) + 12];
	unsigned char *blob;
	int blob_len;
	bool use_mic;
	const unsigned char *p;

	memset(&auth, 0, sizeof(auth));

	/* no timestamp, no MIC, server pairs, terminator and Z(4) */
	assert(TDS_SUCCEED(build_names_blob(&auth, target_info, sizeof(target_info), true, &blob, &blob_len, &use_mic)));
	assert(!use_mic);
	assert(blob_len == PREFIX_LEN + sizeof(target_info) + 4);
	assert(memcmp(blob + PREFIX_LEN, target_info, sizeof(target_info)) == 0);
	assert(TDS_GET_UA4LE(blob + blob_len - 4) == 0);
	free(blob);

	/* add a timestamp, MIC should be used */
	memcpy(info, target_info, TARGET_INFO_PAIRS_LEN);
	memcpy(info + TARGET_INFO_PAIRS_LEN, "\x07\x00\x08\x00\x01\x02\x03\x04\x05\x06\x07\x08\x00\x00\x00\x00", 16);
	auth.has_cbt = true;
	memset(auth.cbt, 0xcc, sizeof(auth.cbt));
	auth.spn = (char *) "S\0P\0N\0";
	auth.spn_len = 6;
	assert(TDS_SUCCEED(build_names_blob(&auth, info, sizeof(info), true, &blob, &blob_len, &use_mic)));
	assert(use_mic);
	/* server timestamp is used */
	check("timestamp", blob + TDS_OFFSET(names_blob_prefix_t, timestamp), 8, "0102030405060708");
	/* server pairs, MsvAvFlags with MIC bit, channel binding, target name, terminator, Z(4) */
	p = blob + PREFIX_LEN + TARGET_INFO_PAIRS_LEN + 12;
	check("added pairs", p, blob + blob_len - p,
	      "0600040002000000" "0a001000cccccccccccccccccccccccccccccccc" "0900060053005000" "4e00" "00000000" "00000000");
	free(blob);

	/* MIC not allowed (NTLMv2 not used) */
	auth.has_cbt = false;
	auth.spn = NULL;
	auth.spn_len = 0;
	assert(TDS_SUCCEED(build_names_blob(&auth, info, sizeof(info), false, &blob, &blob_len, &use_mic)));
	assert(!use_mic);
	assert(blob_len == PREFIX_LEN + sizeof(info) + 4);
	free(blob);

	/* malformed pairs are rejected */
	assert(TDS_FAILED(build_names_blob(&auth, target_info, sizeof(target_info) - 4, true, &blob, &blob_len, &use_mic)));
	memcpy(info, target_info, sizeof(target_info));
	info[2] = 0x40;
	assert(TDS_FAILED(build_names_blob(&auth, info, sizeof(target_info), true, &blob, &blob_len, &use_mic)));
}

static void
test_upper(void)
{
	static const uint16_t tests[][2] = {
		{0x0061, 0x0041},	/* a */
		{0x007a, 0x005a},	/* z */
		{0x0041, 0x0041},	/* A */
		{0x00e9, 0x00c9},	/* e acute */
		{0x00ff, 0x0178},	/* y diaeresis */
		{0x00df, 0x00df},	/* sharp s, no simple mapping */
		{0x0131, 0x0049},	/* dotless i */
		{0x0101, 0x0100},	/* a macron, delta 0xffff range, every other character */
		{0x0100, 0x0100},	/* A macron */
		{0x03c3, 0x03a3},	/* sigma */
		{0x03c2, 0x03a3},	/* final sigma */
		{0x044f, 0x042f},	/* cyrillic ya */
		{0x0451, 0x0401},	/* cyrillic io */
		{0x1f80, 0x1f88},	/* greek alpha with psili and ypogegrammeni */
		{0x1f51, 0x1f59},	/* greek upsilon with dasia, delta 8 every other character */
		{0x1f52, 0x1f52},	/* greek upsilon with psili and varia, no simple mapping */
		{0x1f57, 0x1f5f},	/* greek upsilon with dasia and perispomeni */
		{0xff41, 0xff21},	/* fullwidth a */
		{0xd800, 0xd800},	/* surrogate */
		{0x20ac, 0x20ac},	/* euro sign */
	};
	unsigned int i;
	char buf[4];

	for (i = 0; i < TDS_VECTOR_SIZE(tests); ++i) {
		if (ucs2_upper(tests[i][0]) != tests[i][1]) {
			fprintf(stderr, "Wrong upper case of %04x: got %04x expected %04x\n",
				tests[i][0], ucs2_upper(tests[i][0]), tests[i][1]);
			exit(1);
		}
	}

	/* string conversion, UTF-16LE */
	memcpy(buf, "\xe9\x00\x3c\x04", 4);
	ucs2le_to_upper(buf, sizeof(buf));
	check("UTF-16 upper", buf, sizeof(buf), "c9001c04");
}

TEST_MAIN()
{
	test_upper();
	test_responses();
	test_mic();
	test_names_blob();
	return 0;
}
