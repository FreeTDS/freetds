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

static char software_version[] = "$Id: iconv.c,v 1.58 2003-05-04 19:30:09 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define CHARSIZE(charset) ( ((charset)->min_bytes_per_char == (charset)->max_bytes_per_char )? \
				(charset)->min_bytes_per_char : 0 )

static int bytes_per_char(TDS_ENCODING * charset);
static char *lcid2charset(int lcid);
static int skip_one_input_sequence(iconv_t cd, const TDS_ENCODING * charset, ICONV_CONST char **input, size_t * input_size);
static const char *tds_canonical_charset_name(const char *charset_name);
static const char *tds_sybase_charset_name(const char *charset_name);

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
	TDS_ENCODING *client = &tds->iconv_info->client_charset;
	TDS_ENCODING *server = &tds->iconv_info->server_charset;

#if HAVE_ICONV
	int ratio_c, ratio_s;

	strncpy(client->name, charset, sizeof(client->name));
	client->name[sizeof(client->name) - 1] = '\0';

	tdsdump_log(TDS_DBG_FUNC, "iconv will convert client-side data to the \"%s\" character set\n", charset);

	/* TODO support Sybase dbms and tds4.2 */

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

	tds->iconv_info->to_wire = iconv_open(server->name, client->name);
	if (tds->iconv_info->to_wire == (iconv_t) - 1) {
		tdsdump_log(TDS_DBG_FUNC, "%L iconv_open: cannot convert to \"%s\"\n", charset);
		return;
	}
	tds->iconv_info->from_wire = iconv_open(client->name, server->name);
	if (tds->iconv_info->from_wire == (iconv_t) - 1) {
		tdsdump_log(TDS_DBG_FUNC, "%L iconv_open: cannot convert from \"%s\"\n", charset);
		return;
	}
#else
	strcpy(client->name, "ISO-8859-1");
	strcpy(server->name, "UCS-2LE");

	bytes_per_char(client);
	bytes_per_char(server);
#endif
}

void
tds_iconv_close(TDSSOCKET * tds)
{
#if HAVE_ICONV
	if (tds->iconv_info->to_wire != (iconv_t) - 1) {
		iconv_close(tds->iconv_info->to_wire);
		tds->iconv_info->to_wire = (iconv_t) - 1;
	}

	if (tds->iconv_info->from_wire != (iconv_t) - 1) {
		iconv_close(tds->iconv_info->from_wire);
		tds->iconv_info->from_wire = (iconv_t) - 1;
	}
#endif
}

/**
 * \retval number of bytes placed in \a output
 * \todo Check for variable multibyte non-UTF-8 input character set.  
 */
size_t
tds_iconv(TDS_ICONV_DIRECTION io, const TDSICONVINFO * iconv_info, const char *input, size_t * input_size,
	  char *output, size_t output_size)
{
#if HAVE_ICONV
	const TDS_ENCODING *input_charset = NULL;
	const char *output_charset_name = NULL;
	const size_t output_buffer_size = output_size;
	int erc, one_character;
	ICONV_CONST char *input_p = (ICONV_CONST char *) input;

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

	if (cd == (iconv_t) - 1)	/* FIXME: call memcpy, adjust input and *input_size, and return copied size */
		return 0;

	/*
	 * Call iconv() as many times as necessary, until we reach the end of input 
	 * or exhaust output.  
	 */
	while (iconv(cd, &input_p, input_size, &output, &output_size) == (size_t) - 1) {
		/* iconv call can reset errno */
		erc = errno;
		/* reset iconv state */
		iconv(cd, NULL, NULL, NULL, NULL);
		if (erc != EILSEQ)
			break;

		/* skip one input sequence, adjusting input pointer */
		one_character = skip_one_input_sequence(cd, input_charset, &input_p, input_size);
		*input_size -= one_character;

		if (!one_character)
			break;	/* Unknown charset, what to do?  I prefer "assert(one_charset)" --jkl */

		/* 
		 * To replace invalid input with '?', we have to convert an ASCII '?' 
		 * into the output character set.  In unimaginably weird circumstances, this might  be
		 * impossible.  
		 */
		if (error_cd == (iconv_t) - 1) {
			/* TODO is ascii extension just copy (always ascii extension??) */
			/* FIXME use iconv name for UTF-8 (some platform use different names) */
			/* traslation to every charset is handled in UTF-8 and UCS-2 */
			error_cd = iconv_open(output_charset_name, "UTF-8");
			if (error_cd == (iconv_t) - 1)
				break;	/* what to do? */
		}
		lquest_mark = 1;
		pquest_mark = quest_mark;

		iconv(error_cd, &pquest_mark, &lquest_mark, &output, &output_size);

		/* FIXME this can happen if output buffer is too small... */
		if (output_size == 0)
			break;
	}

	if (error_cd != (iconv_t) - 1)
		iconv_close(error_cd);

	return output_buffer_size - output_size;
#else
	/* FIXME best code, please, this do not convert unicode <-> singlebyte */
	if (output_size > *input_size)
		output_size = *input_size;
	memcpy(output, input, output_size);
	*input_size -= output_size;
	return output_size;
#endif
}

/* FIXME should change only singlebyte conversions */
void
tds7_srv_charset_changed(TDSSOCKET * tds, int lcid)
{
#if HAVE_ICONV
	int ret;

	const char *charset = lcid2charset(lcid);

	strcpy(tds->iconv_info->server_charset.name, charset);

	/* 
	 * Close any previously opened iconv descriptors. 
	 */
	if (tds->iconv_info->to_wire != (iconv_t) - 1)
		iconv_close(tds->iconv_info->to_wire);

	if (tds->iconv_info->from_wire != (iconv_t) - 1)
		iconv_close(tds->iconv_info->from_wire);

	/* TODO test before ? - f77 */
	/* look up the size of the server's new character set */
	ret = bytes_per_char(&tds->iconv_info->server_charset);
	if (!ret) {
		tdsdump_log(TDS_DBG_FUNC, "%L tds7_srv_charset_changed: cannot convert to \"%s\"\n", charset);
		tds->iconv_info->to_wire = (iconv_t) - 1;
		tds->iconv_info->from_wire = (iconv_t) - 1;
		return;
	}


	tds->iconv_info->to_wire = iconv_open(tds->iconv_info->server_charset.name, tds->iconv_info->client_charset.name);

	tds->iconv_info->from_wire = iconv_open(tds->iconv_info->client_charset.name, tds->iconv_info->server_charset.name);
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
	static const TDS_ENCODING charsets[] = {
#include "character_sets.h"
	};
	int i;

	assert(charset && strlen(charset->name) < sizeof(charset->name));

	for (i = 0; i < sizeof(charsets) / sizeof(TDS_ENCODING); i++) {
		if (charsets[i].min_bytes_per_char == 0)
			break;

		if (0 == strcmp(charset->name, charsets[i].name)) {
			charset->min_bytes_per_char = charsets[i].min_bytes_per_char;
			charset->max_bytes_per_char = charsets[i].max_bytes_per_char;

			return (charset->max_bytes_per_char == charset->min_bytes_per_char) ? 1 : 2;
		}
	}

	return 0;
}

/**
 * Move the input sequence pointer to the next valid position.
 * Used when an input character cannot be converted.  
 * \returns number of bytes to skip.
 */
/* FIXME possible buffer reading overflow ?? */
static int
skip_one_input_sequence(iconv_t cd, const TDS_ENCODING * charset, ICONV_CONST char **input, size_t * input_size)
{
	int charsize = CHARSIZE(charset);
	char ib[16];
	char ob[16];
	char *pib, *pob;
	size_t il, ol, l;
	iconv_t cd2;


	/* usually fixed size and UTF-8 do not have state, so do not reset it */
	if (charsize) {
		*input += charsize;
		*input_size -= charsize;
		return charsize;
	}

	if (0 == strcmp(charset->name, "UTF-8")) {
		/* Deal with UTF-8.  
		 * bytes | bits | representation
		 *     1 |    7 | 0vvvvvvv
		 *     2 |   11 | 110vvvvv 10vvvvvv
		 *     3 |   16 | 1110vvvv 10vvvvvv 10vvvvvv
		 *     4 |   21 | 11110vvv 10vvvvvv 10vvvvvv 10vvvvvv
		 */
		int c = **input;

		c = c & (c >> 1);
		do {
			++charsize;
		} while ((c <<= 1) & 0x80);
		*input += charsize;
		*input_size += charsize;
		return charsize;
	}

	/* handle state encoding */

	/* extract state from iconv */
	pob = ib;
	ol = sizeof(ib);
	iconv(cd, NULL, NULL, &pob, &ol);

	/* init destination conversion */
	cd2 = iconv_open("UTF-8", charset->name);
	if (cd2 == (iconv_t) - 1)
		return 0;

	/* add part of input */
	il = ol;
	if (il > *input_size)
		il = *input_size;
	l = sizeof(ib) - ol;
	memcpy(ib + l, *input, il);
	il += l;

	/* translate a characters */
	pib = ib;
	pob = ob;
	ol = sizeof(ob);
	iconv(cd2, &pib, &il, &pob, &ol);

	/* adjust input */
	l = (pib - ib) - sl;
	*input += l;
	*input_size -= l;

	/* extract state */
	pob = ib;
	ol = sizeof(ib);
	iconv(cd, NULL, NULL, &pob, &ol);

	/* set input state */
	pib = ib;
	ib = sizeof(ib) - ol;
	pob = ob;
	ol = sizeof(ob);
	iconv(cd, &pib, &il, &pob, &ol);

	iconv_close(cd2);

	return l;
}

static const char *
lookup_charset_name(const CHARACTER_SET_ALIAS aliases[], const char *charset_name)
{
	int i;

	if (!charset_name || *charset_name == '\0')
		return charset_name;

	for (i = 0; i < sizeof(aliases) / sizeof(CHARACTER_SET_ALIAS); i++) {
		if (aliases[i].alias == 0)
			break;

		if (0 == strcmp(charset_name, aliases[i].alias)) {
			return aliases[i].name;
		}
	}

	return NULL;
}

/**
 * Determine canonical iconv character set name.  
 * \returns canonical name, or NULL if lookup failed.
 * \remarks Returned name can be used in bytes_per_char(), above.
 */
static const char *
tds_canonical_charset_name(const char *charset_name)
{
	static const CHARACTER_SET_ALIAS aliases[] = {
#		include "alternative_character_sets.h"
		,
#		include "sybase_character_sets.h"
	};

	return lookup_charset_name(aliases, charset_name);
}

/**
 * Determine the name Sybase uses for a character set, given a canonical iconv name.  
 * \returns Sybase name, or NULL if lookup failed.
 * \remarks Returned name can be sent to Sybase a server.
 */
static const char *
tds_sybase_charset_name(const char *charset_name)
{
	static const CHARACTER_SET_ALIAS aliases[] = {
#		include "sybase_character_sets.h"
	};

	return lookup_charset_name(aliases, charset_name);
}

static char *
lcid2charset(int lcid)
{
	/* The table from the MSQLServer reference "Windows Collation Designators" 
	 * and from " NLS Information for Microsoft Windows XP"
	 */

	char *cp = NULL;

	/* TODO consider only lower 16 bit ?? */
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

/** \@} */
