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

static char software_version[] = "$Id: iconv.c,v 1.45 2003-03-30 08:50:15 freddy77 Exp $";
static void *no_unused_var_warn[] = {
	software_version,
	no_unused_var_warn
};

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
	TDSICONVINFO *iconv_info;

	iconv_info = (TDSICONVINFO *) tds->iconv_info;

#if HAVE_ICONV
	iconv_info->bytes_per_char = 1;
	strncpy(iconv_info->client_charset, strdup(charset), sizeof(iconv_info->client_charset));
	iconv_info->client_charset[sizeof(iconv_info->client_charset) - 1] = '\0';
	tdsdump_log(TDS_DBG_FUNC, "iconv will convert client-side data to the \"%s\" character set\n", charset);
	iconv_info->cdto_ucs2 = iconv_open("UCS-2LE", charset);
	if (iconv_info->cdto_ucs2 == (iconv_t) - 1) {
		iconv_info->use_iconv = 0;
		tdsdump_log(TDS_DBG_FUNC, "%L iconv_open: cannot convert to \"%s\"\n", charset);
		return;
	}
	iconv_info->cdfrom_ucs2 = iconv_open(charset, "UCS-2LE");
	if (iconv_info->cdfrom_ucs2 == (iconv_t) - 1) {
		iconv_info->use_iconv = 0;
		tdsdump_log(TDS_DBG_FUNC, "%L iconv_open: cannot convert from \"%s\"\n", charset);
		return;
	}
	/* TODO init singlebyte server */
	if (strcasecmp(charset, "utf8") == 0 || strcasecmp(charset, "utf-8") == 0)
		/* 3 bytes per characters should be sufficient */
		iconv_info->bytes_per_char = 3;
	iconv_info->use_iconv = 1;
#else
	iconv_info->use_iconv = 0;
	tdsdump_log(TDS_DBG_FUNC, "%L iconv library not employed, relying on ISO-8859-1 compatibility\n");
#endif
}

void
tds_iconv_close(TDSSOCKET * tds)
{
	TDSICONVINFO *iconv_info;

	iconv_info = (TDSICONVINFO *) tds->iconv_info;

#if HAVE_ICONV
	if (iconv_info->cdto_ucs2 != (iconv_t) - 1) {
		iconv_close(iconv_info->cdto_ucs2);
	}
	if (iconv_info->cdfrom_ucs2 != (iconv_t) - 1) {
		iconv_close(iconv_info->cdfrom_ucs2);
	}
	if (iconv_info->cdto_srv != (iconv_t) - 1) {
		iconv_close(iconv_info->cdto_srv);
	}
	if (iconv_info->cdfrom_srv != (iconv_t) - 1) {
		iconv_close(iconv_info->cdfrom_srv);
	}
#endif
}

void
tds7_srv_charset_changed(TDSSOCKET * tds, int lcid)
{
#if HAVE_ICONV
	char *cp;
	TDSICONVINFO *iconv_info;
	iconv_t tmp_cd;

	iconv_info = (TDSICONVINFO *) tds->iconv_info;

	/* The table from the MSQLServer reference "Windows Collation Designators" 
	 * and from " NLS Information for Microsoft Windows XP"
	 */
	switch (lcid) {
	case 0x1040e: /* FIXME check, in neither table but returned from mssql2k */
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
	case 0x10412: /* FIXME check, in neither table but returned from mssql2k */
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

	tmp_cd = iconv_open(cp, iconv_info->client_charset);
	if (tmp_cd != (iconv_t) - 1) {
		if (iconv_info->cdto_srv != (iconv_t) - 1)
			iconv_close(iconv_info->cdto_srv);
		iconv_info->cdto_srv = tmp_cd;
	}

	tmp_cd = iconv_open(iconv_info->client_charset, cp);
	if (tmp_cd != (iconv_t) - 1) {
		if (iconv_info->cdfrom_srv != (iconv_t) - 1)
			iconv_close(iconv_info->cdfrom_srv);
		iconv_info->cdfrom_srv = tmp_cd;
	}
#endif
}

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

/** \@} */
