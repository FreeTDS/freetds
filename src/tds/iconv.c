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

/*
 * iconv.c, handle all the conversion stuff without spreading #if HAVE_ICONV 
 * all over the other code
 */

#include <assert.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#if HAVE_ERRNO_H
#include <errno.h>
#endif

#include "tds.h"
#include "tdsiconv.h"
#if HAVE_ICONV
#include <iconv.h>
#endif
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: iconv.c,v 1.47 2003-04-06 20:34:57 jklowden Exp $";
static void *no_unused_var_warn[] = {
	software_version,
	no_unused_var_warn
};

static int bytes_per_char(TDS_ENCODING * charset);
static char *lcid2charset(int lcid);

/**
 * \ingroup libtds
 * \defgroup conv Charset conversion
 * Convert between different charsets
 */

/**
 * \addtogroup conv
 * \@{ 
 */

void
tds_iconv_open(TDSSOCKET * tds, char *charset)
{
	TDS_ENCODING *client = &tds->iconv_info.client_charset;
	TDS_ENCODING *server = &tds->iconv_info.server_charset;
	int ratio_c, ratio_s;

	if (tds->iconv_info.to_wire == (iconv_t) - 1 || tds->iconv_info.from_wire == (iconv_t) - 1) {
		tdsdump_log(TDS_DBG_FUNC, "%L iconv library not employed, relying on ISO-8859-1 compatibility\n");
		return;
	}
#if HAVE_ICONV
	strncpy(client->name, charset, sizeof(client->name));
	client->name[sizeof(client->name) - 1] = '\0';

	tdsdump_log(TDS_DBG_FUNC, "iconv will convert client-side data to the \"%s\" character set\n", charset);

	/* TODO init singlebyte server */
	strncpy(server->name, "UCS-2LE", sizeof(server->name));
	server->name[sizeof(server->name) - 1] = '\0';

	ratio_c = bytes_per_char(client);
	if (ratio_c == 0)
		return;		/* client set unknown */

	ratio_s = bytes_per_char(server);
	if (ratio_s == 0)
		return;		/* server set unknown */

	/* 
	 * How many UTF-8 bytes we need is a function of what the input character set is.
	 * TODO This could definitely be more sophisticated, but it deals with the common case.
	 */
	if (client->min_bytes_per_char == 1 && client->max_bytes_per_char == 4 && server->max_bytes_per_char == 1) {
		/* ie client is UTF-8 and server is ISO-8859-1 or variant. */
		client->max_bytes_per_char = 3;
	}

	tds->iconv_info.to_wire = iconv_open(server->name, client->name);
	if (tds->iconv_info.to_wire == (iconv_t) - 1) {
		tdsdump_log(TDS_DBG_FUNC, "%L iconv_open: cannot convert to \"%s\"\n", charset);
		return;
	}
	tds->iconv_info.from_wire = iconv_open(client->name, server->name);
	if (tds->iconv_info.from_wire == (iconv_t) - 1) {
		tdsdump_log(TDS_DBG_FUNC, "%L iconv_open: cannot convert from \"%s\"\n", charset);
		return;
	}
#endif
}

void
tds_iconv_close(TDSSOCKET * tds)
{
#if HAVE_ICONV
	if (tds->iconv_info.to_wire != (iconv_t) - 1) {
		iconv_close(tds->iconv_info.to_wire);
	}

	if (tds->iconv_info.from_wire != (iconv_t) - 1) {
		iconv_close(tds->iconv_info.from_wire);
	}
#endif
}

/**
 * \retval number of bytes placed in \a output
 * \todo Check for variable multibyte non-UTF-8 input character set.  
 */
size_t
tds_iconv(TDS_ICONV_DIRECTION io, const TDSICONVINFO * iconv_info, ICONV_CONST char *input, size_t * input_size,
	  char *output, size_t output_size)
{
	const TDS_ENCODING *input_charset = NULL;
	const char *output_charset_name = NULL;

	int erc;

	iconv_t cd = (iconv_t) - 1, error_cd = (iconv_t) - 1;

	char quest_mark[] = "?";	/* best to leave non-const; implementations vary */
	ICONV_CONST char *pquest_mark = quest_mark;
	int lquest_mark;

	switch (io) {
	case to_server:
		cd = iconv_info->to_wire;
		input_charset = &iconv_info->client_charset;
		output_charset_name = iconv_info->server_charset.name;
		break;
	case to_client:
		cd = iconv_info->from_wire;
		input_charset = &iconv_info->server_charset;
		output_charset_name = iconv_info->client_charset.name;
		break;
	default:
		cd = (iconv_t) - 1;
		break;
	}

	if (cd == (iconv_t) - 1)
		return 0;

	/*
	 * Call iconv() as many times as necessary, until we reach the end of input 
	 * or exhaust output.  
	 */
	while (iconv(cd, &input, input_size, &output, &output_size) == (size_t) - 1) {
		/* iconv call can reset errno */
		erc = errno;
		/* reset iconv state */
		iconv(cd, NULL, NULL, NULL, NULL);
		if (erc != EILSEQ)
			break;

		/* skip one input sequence */
		if (input_charset->min_bytes_per_char == input_charset->min_bytes_per_char) {
			input += input_charset->min_bytes_per_char;
			input_size -= input_charset->min_bytes_per_char;
		} else {
			/* deal with UTF-8.  Others will just break :-( 
			 * bytes | bits | representation
			 *     1 |    7 | 0vvvvvvv
			 *     2 |   11 | 110vvvvv 10vvvvvv
			 *     3 |   16 | 1110vvvv 10vvvvvv 10vvvvvv
			 *     4 |   21 | 11110vvv 10vvvvvv 10vvvvvv 10vvvvvv
			 */
			while (*input | 0x80) {
				input++;
				input_size--;
			}

		}

		/* 
		 * To replace invalid intput with '?', we have to convert an ASCII '?' 
		 * into the output character set.  In unimaginably weird circumstances, this might  be
		 * impossible.  
		 */
		if (error_cd == (iconv_t) - 1) {
			error_cd = iconv_open(output_charset_name, "ISO-8859-1");
			if (error_cd == (iconv_t) - 1)
				break;	/* what to do? */
		}
		lquest_mark = sizeof(quest_mark) - 1;
		pquest_mark = quest_mark;

		iconv(error_cd, &pquest_mark, &lquest_mark, &output, &output_size);

		if (output_size == 0)
			break;
	}

	if (error_cd != (iconv_t) - 1)
		iconv_close(error_cd);

	return output_size;
}

void
tds7_srv_charset_changed(TDSSOCKET * tds, int lcid)
{
#if HAVE_ICONV
	int ret;

	const char *charset = lcid2charset(lcid);

	strcpy(tds->iconv_info.server_charset.name, charset);

	/* 
	 * Close any previously opened iconv descriptors. 
	 */
	if (tds->iconv_info.to_wire != (iconv_t) - 1)
		iconv_close(tds->iconv_info.to_wire);

	if (tds->iconv_info.from_wire != (iconv_t) - 1)
		iconv_close(tds->iconv_info.from_wire);

	/* look up the size of the server's new character set */
	ret = bytes_per_char(&tds->iconv_info.server_charset);
	if (!ret) {
		tdsdump_log(TDS_DBG_FUNC, "%L tds7_srv_charset_changed: cannot convert to \"%s\"\n", charset);
		tds->iconv_info.to_wire = (iconv_t) - 1;
		tds->iconv_info.from_wire = (iconv_t) - 1;
		return;
	}


	tds->iconv_info.to_wire = iconv_open(tds->iconv_info.server_charset.name, tds->iconv_info.client_charset.name);

	tds->iconv_info.from_wire = iconv_open(tds->iconv_info.client_charset.name, tds->iconv_info.server_charset.name);
#endif
}

/**
 * Determine byte/char for an iconv character set.  
 * \retval 0 failed, no such charset.
 * \retval 1 succeeded, fixed byte/char.
 * \retval 2 succeeded, variable byte/char.
 */
static int
bytes_per_char(TDS_ENCODING * charset)
{
	TDS_ENCODING charsets[] = {
#include "character_sets.h"
	};
	int i;

	assert(charset && strlen(charset->name) < sizeof(charset->name));

	for (i = 0; i < sizeof(charsets) / sizeof(TDS_ENCODING); i++) {
		if (charsets[1].min_bytes_per_char == 0)
			break;

		if (0 == strcmp(charset->name, charsets[i].name)) {
			charset->min_bytes_per_char = charsets[i].min_bytes_per_char;
			charset->max_bytes_per_char = charsets[i].max_bytes_per_char;

			return (charset->max_bytes_per_char == charset->min_bytes_per_char) ? 1 : 2;
		}
	}

	return 0;
}

static char *
lcid2charset(int lcid)
{
	/* The table from the MSQLServer reference "Windows Collation Designators" 
	 * and from " NLS Information for Microsoft Windows XP"
	 */

	char *cp = NULL;

	switch (lcid) {
	case 0x1040e:		/* FIXME check, in neither table but returned from mssql2k */
	case 0x405:
	case 0x40e:
	case 0x415:
	case 0x418:
	case 0x41a:
	case 0x41b:
	case 0x41c:
	case 0x424:
		/* case 0x81a: *//* seem wrong in XP table TODO check */
	case 0x104e:		/* ?? */
		cp = "CP1250";
		break;
	case 0x402:
	case 0x419:
	case 0x422:
	case 0x423:
	case 0x42f:
	case 0x43f:
	case 0x440:
	case 0x444:
	case 0x450:
	case 0x81a:		/* ?? */
	case 0x82c:
	case 0x843:
	case 0xc1a:
		cp = "CP1251";
		break;
	case 0x1007:
	case 0x1009:
	case 0x100a:
	case 0x100c:
	case 0x1407:
	case 0x1409:
	case 0x140a:
	case 0x140c:
	case 0x10407:
	case 0x10437:
	case 0x1809:
	case 0x180a:
	case 0x180c:
	case 0x1c09:
	case 0x1c0a:
	case 0x2009:
	case 0x200a:
	case 0x2409:
	case 0x240a:
	case 0x2809:
	case 0x280a:
	case 0x2c09:
	case 0x2c0a:
	case 0x3009:
	case 0x300a:
	case 0x3409:
	case 0x340a:
	case 0x380a:
	case 0x3c0a:
	case 0x400a:
	case 0x403:
	case 0x406:
	case 0x407:
	case 0x409:
	case 0x40a:
	case 0x40b:
	case 0x40c:
	case 0x40f:
	case 0x410:
	case 0x413:
	case 0x414:
	case 0x416:
	case 0x41d:
	case 0x421:
	case 0x42d:
	case 0x436:
	case 0x438:
		/*case 0x439:  *//*??? Unicode only */
	case 0x43e:
	case 0x440a:
	case 0x441:
	case 0x456:
	case 0x480a:
	case 0x4c0a:
	case 0x500a:
	case 0x807:
	case 0x809:
	case 0x80a:
	case 0x80c:
	case 0x810:
	case 0x813:
	case 0x814:
	case 0x816:
	case 0x81d:
	case 0x83e:
	case 0xc07:
	case 0xc09:
	case 0xc0a:
	case 0xc0c:
		cp = "CP1252";
		break;
	case 0x408:
		cp = "CP1253";
		break;
	case 0x41f:
	case 0x42c:
	case 0x443:
		cp = "CP1254";
		break;
	case 0x40d:
		cp = "CP1255";
		break;
	case 0x1001:
	case 0x1401:
	case 0x1801:
	case 0x1c01:
	case 0x2001:
	case 0x2401:
	case 0x2801:
	case 0x2c01:
	case 0x3001:
	case 0x3401:
	case 0x3801:
	case 0x3c01:
	case 0x4001:
	case 0x401:
	case 0x420:
	case 0x429:
	case 0x801:
	case 0xc01:
		cp = "CP1256";
		break;
	case 0x425:
	case 0x426:
	case 0x427:
	case 0x827:		/* ?? */
		cp = "CP1257";
		break;
	case 0x42a:
		cp = "CP1258";
		break;
	case 0x41e:
		cp = "CP874";
		break;
	case 0x411:
	case 0x10411:
		cp = "CP932";
		break;
	case 0x1004:
	case 0x20804:
	case 0x804:
		cp = "CP936";
		break;
	case 0x10412:		/* FIXME check, in neither table but returned from mssql2k */
	case 0x412:
		cp = "CP949";
		break;
	case 0x1404:
	case 0x30404:
	case 0x404:
	case 0xc04:
		cp = "CP950";
		break;
	default:
		cp = "CP1252";
	}

	assert(cp);
	return cp;
}

#if 0
/**
 * convert from ucs2 string to ascii.
 * @return saved bytes
 * @param in_string ucs2 string (not terminated) to convert to ascii
 * @param in_len length of input string in characters (2 byte)
 * @param out_string buffer to store translated string. It should be large enough 
 *        to handle out_len bytes. string won't be zero terminated.
 * @param out_len length of input string in characters
 */
int
tds7_unicode2ascii(TDSSOCKET * tds, const char *in_string, int in_len, char *out_string, int out_len)
{
	int i;

#if HAVE_ICONV
	TDSICONVINFO *iconv_info;
	ICONV_CONST char *in_ptr;
	char *out_ptr;
	size_t out_bytes, in_bytes;
	char quest_mark[] = "?\0";	/* best to live no-const */
	ICONV_CONST char *pquest_mark;
	size_t lquest_mark;
#endif

	if (!in_string)
		return 0;

#if HAVE_ICONV
	iconv_info = (TDSICONVINFO *) tds->iconv_info;
	if (iconv_info->use_iconv) {
		out_bytes = out_len;
		in_bytes = in_len * 2;
		in_ptr = (ICONV_CONST char *) in_string;
		out_ptr = out_string;
		while (iconv(iconv_info->cdfrom_ucs2, &in_ptr, &in_bytes, &out_ptr, &out_bytes) == (size_t) - 1) {
			/* iconv call can reset errno */
			i = errno;
			/* reset iconv state */
			iconv(iconv_info->cdfrom_ucs2, NULL, NULL, NULL, NULL);
			if (i != EILSEQ)
				break;

			/* skip one UCS-2 sequence */
			in_ptr += 2;
			in_bytes -= 2;

			/* replace invalid with '?' */
			pquest_mark = quest_mark;
			lquest_mark = 2;
			iconv(iconv_info->cdfrom_ucs2, &pquest_mark, &lquest_mark, &out_ptr, &out_bytes);
			if (out_bytes == 0)
				break;
		}
		return out_len - out_bytes;
	}
#endif

	/* no iconv, strip high order byte if zero or replace with '?' 
	 * this is the same of converting to ISO8859-1 charset using iconv */
	/* TODO update docs */
	if (out_len < in_len)
		in_len = out_len;
	for (i = 0; i < in_len; ++i) {
		out_string[i] = in_string[i * 2 + 1] ? '?' : in_string[i * 2];
	}
	return in_len;
}

/**
 * convert a ascii string to ucs2.
 * Note: output string is not terminated
 * @param in_string string to translate, null terminated
 * @param out_string buffer to store translated string
 * @param maxlen length of out_string buffer in bytes
 */
char *
tds7_ascii2unicode(TDSSOCKET * tds, const char *in_string, char *out_string, int maxlen)
{
	register int out_pos = 0;
	register int i;
	size_t string_length;

#if HAVE_ICONV
	TDSICONVINFO *iconv_info;
	ICONV_CONST char *in_ptr;
	char *out_ptr;
	size_t out_bytes, in_bytes;
#endif

	if (!in_string)
		return NULL;
	string_length = strlen(in_string);

#if HAVE_ICONV
	iconv_info = (TDSICONVINFO *) tds->iconv_info;
	if (iconv_info->use_iconv) {
		out_bytes = maxlen;
		in_bytes = string_length;
		in_ptr = (ICONV_CONST char *) in_string;
		out_ptr = out_string;
		iconv(iconv_info->cdto_ucs2, &in_ptr, &in_bytes, &out_ptr, &out_bytes);

		return out_string;
	}
#endif

	/* no iconv, add null high order byte to convert 7bit ascii to unicode */
	if (string_length * 2 > maxlen)
		string_length = maxlen >> 1;

	for (i = 0; i < string_length; i++) {
		out_string[out_pos++] = in_string[i];
		out_string[out_pos++] = '\0';
	}

	return out_string;
}
#endif
/** \@} */
