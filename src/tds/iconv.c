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
 * iconv.c, handle all the conversion stuff without spreading #if HAVE_ICONV_ALWAYS 
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

/* define this for now; remove when done testing */
#define HAVE_ICONV_ALWAYS 1

static char software_version[] = "$Id: iconv.c,v 1.101 2003-12-23 21:14:19 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define CHARSIZE(charset) ( ((charset)->min_bytes_per_char == (charset)->max_bytes_per_char )? \
				(charset)->min_bytes_per_char : 0 )

#define SAFECPY(d, s) 	strncpy((d), (s), sizeof(d)); (d)[sizeof(d) - 1] = '\0'


#if !HAVE_ICONV_ALWAYS
static int bytes_per_char(TDS_ENCODING * charset);
#endif
static const char *lcid2charset(int lcid);
static int skip_one_input_sequence(iconv_t cd, const TDS_ENCODING * charset, const char **input, size_t * input_size);
static int tds_iconv_info_init(TDSICONVINFO * iconv_info, const char *client_name, const char *server_name);
static int tds_iconv_init(void);
static int tds_canonical_charset(const char *charset_name);
static void _iconv_close(iconv_t * cd);
static void tds_iconv_info_close(TDSICONVINFO * iconv_info);


/**
 * \ingroup libtds
 * \defgroup conv Charset conversion
 * Convert between different charsets.
 */


#include "encodings.h"

/* this will contain real iconv names */
static const char *iconv_names[sizeof(canonic_charsets) / sizeof(canonic_charsets[0])];
static int iconv_initialized = 0;
static unsigned int ucs2pos;
static const char *ucs2name;

enum
{ POS_ISO1, POS_UTF8, POS_UCS2LE, POS_UCS2BE };

/**
 * Initialize charset searching for UTF-8, UCS-2 and ISO8859-1
 */
static int
tds_iconv_init(void)
{
	int i;
	iconv_t cd;

	/* first entries should be constants */
	assert(strcmp(canonic_charsets[POS_ISO1].name, "ISO-8859-1") == 0);
	assert(strcmp(canonic_charsets[POS_UTF8].name, "UTF-8") == 0);
	assert(strcmp(canonic_charsets[POS_UCS2LE].name, "UCS-2LE") == 0);
	assert(strcmp(canonic_charsets[POS_UCS2BE].name, "UCS-2BE") == 0);

	/* fast tests for GNU-iconv */
	cd = iconv_open("ISO-8859-1", "UTF-8");
	if (cd != (iconv_t) - 1) {
		iconv_names[POS_ISO1] = "ISO-8859-1";
		iconv_names[POS_UTF8] = "UTF-8";
		iconv_close(cd);
	} else {

		/* search names for ISO8859-1 and UTF-8 */
		for (i = 0; iconv_aliases[i].alias; ++i) {
			int j;

			if (iconv_aliases[i].canonic != POS_ISO1)
				continue;
			for (j = 0; iconv_aliases[j].alias; ++j) {
				if (iconv_aliases[j].canonic != POS_UTF8)
					continue;

				cd = iconv_open(iconv_aliases[i].alias, iconv_aliases[j].alias);
				if (cd != (iconv_t) - 1) {
					iconv_names[POS_ISO1] = iconv_aliases[i].alias;
					iconv_names[POS_UTF8] = iconv_aliases[j].alias;
					iconv_close(cd);
					break;
				}
			}
			if (iconv_names[POS_ISO1])
				break;
		}
		/* required characters not found !!! */
		if (!iconv_names[POS_ISO1])
			return 1;
	}

	/* now search for UCS-2 */
	cd = iconv_open(iconv_names[POS_ISO1], "UCS-2LE");
	if (cd != (iconv_t) - 1) {
		iconv_names[POS_UCS2LE] = "UCS-2LE";
		iconv_close(cd);
	}
	cd = iconv_open(iconv_names[POS_ISO1], "UCS-2BE");
	if (cd != (iconv_t) - 1) {
		iconv_names[POS_UCS2BE] = "UCS-2BE";
		iconv_close(cd);
	}

	/* long search needed ?? */
	if (!iconv_names[POS_UCS2LE] || !iconv_names[POS_UCS2BE]) {
		for (i = 0; iconv_aliases[i].alias; ++i) {
			if (strncmp(canonic_charsets[iconv_aliases[i].canonic].name, "UCS-2", 5) != 0)
				continue;

			cd = iconv_open(iconv_aliases[i].alias, iconv_names[POS_ISO1]);
			if (cd != (iconv_t) - 1) {
				char ib[1];
				char ob[4];
				size_t il, ol;
				ICONV_CONST char *pib;
				char *pob;
				int byte_sequence = 0;

				/* try to convert 'A' and check result */
				ib[0] = 0x41;
				pib = ib;
				pob = ob;
				il = 1;
				ol = 4;
				ob[0] = ob[1] = 0;
				if (iconv(cd, &pib, &il, &pob, &ol) != (size_t) - 1) {
					/* byte order sequence ?? */
					if (ol == 0) {
						ob[0] = ob[2];
						byte_sequence = 1;
						/* TODO save somewhere */
					}

					/* save name without sequence (if present) */
					if (ob[0])
						il = POS_UCS2LE;
					else
						il = POS_UCS2BE;
					if (!iconv_names[il] || !byte_sequence)
						iconv_names[il] = iconv_aliases[i].alias;
				}
				iconv_close(cd);
			}
		}
	}
	/* we need a UCS-2 (big endian or little endian) */
	if (!iconv_names[POS_UCS2LE] && !iconv_names[POS_UCS2BE])
		return 1;

	ucs2pos = iconv_names[POS_UCS2LE] ? POS_UCS2LE : POS_UCS2BE;
	ucs2name = iconv_names[ucs2pos];

	for (i = 0; i < 4; ++i)
		tdsdump_log(TDS_DBG_INFO1, "names for %s: %s\n", canonic_charsets[i].name,
			    iconv_names[i] ? iconv_names[i] : "(null)");

	/* success (it should always occurs) */
	return 0;
}

/**
 * Get iconv name given canonic
 */
static void
tds_get_iconv_name(int charset)
{
	int i;
	iconv_t cd;

	assert(iconv_initialized);

	/* try using canonic name and UTF-8 and UCS2 */
	cd = iconv_open(iconv_names[POS_UTF8], canonic_charsets[charset].name);
	if (cd != (iconv_t) - 1) {
		iconv_names[charset] = canonic_charsets[charset].name;
		iconv_close(cd);
		return;
	}
	cd = iconv_open(ucs2name, canonic_charsets[charset].name);
	if (cd != (iconv_t) - 1) {
		iconv_names[charset] = canonic_charsets[charset].name;
		iconv_close(cd);
		return;
	}

	/* try all alternatives */
	for (i = 0; iconv_aliases[i].alias; ++i) {
		if (iconv_aliases[i].canonic != charset)
			continue;

		cd = iconv_open(iconv_names[POS_UTF8], iconv_aliases[i].alias);
		if (cd != (iconv_t) - 1) {
			iconv_names[charset] = iconv_aliases[i].alias;
			iconv_close(cd);
			return;
		}

		cd = iconv_open(ucs2name, iconv_aliases[i].alias);
		if (cd != (iconv_t) - 1) {
			iconv_names[charset] = iconv_aliases[i].alias;
			iconv_close(cd);
			return;
		}
	}

	/* charset not found, use memcpy */
	iconv_names[charset] = "";
}

/**
 * Allocate iconv stuff
 * \return 0 for success
 */
int
tds_iconv_alloc(TDSSOCKET * tds)
{
	int i;
	TDSICONVINFO *iconv_info;

	assert(!tds->iconv_info);
	if (!(tds->iconv_info = (TDSICONVINFO **) malloc(sizeof(TDSICONVINFO *) * (initial_iconv_info_count + 1))))
		return 1;
	iconv_info = (TDSICONVINFO *) malloc(sizeof(TDSICONVINFO) * initial_iconv_info_count);
	if (!iconv_info) {
		free(tds->iconv_info);
		tds->iconv_info = NULL;
		return 1;
	}
	memset(iconv_info, 0, sizeof(TDSICONVINFO) * initial_iconv_info_count);
	tds->iconv_info_count = initial_iconv_info_count + 1;

	for (i = 0; i < initial_iconv_info_count; ++i) {
		tds->iconv_info[i] = &iconv_info[i];
		iconv_info[i].server_charset.name = iconv_info[i].client_charset.name = "";
		iconv_info[i].to_wire = (iconv_t) - 1;
		iconv_info[i].to_wire2 = (iconv_t) - 1;
		iconv_info[i].from_wire = (iconv_t) - 1;
		iconv_info[i].from_wire2 = (iconv_t) - 1;
	}

	/* chardata is just a pointer to another iconv info */
	tds->iconv_info[initial_iconv_info_count] = tds->iconv_info[client2server_chardata];

	return 0;
}

/**
 * \addtogroup conv
 * \@{ 
 * Set up the initial iconv conversion descriptors.
 * When the socket is allocated, three TDSICONVINFO structures are attached to iconv_info.  
 * They have fixed meanings:
 * 	\li 0. Client <-> UCS-2 (client2ucs2)
 * 	\li 1. Client <-> server single-byte charset (client2server_chardata)
 *	\li 2. ISO8859-1  <-> server meta data	(iso2server_metadata)
 *
 * Other designs that use less data are possible, but these three conversion needs are 
 * very often needed.  By reserving them, we avoid searching the array for our most common purposes.
 *
 * To solve different iconv names and portability problem FreeTDS use a complex
 * method. It maintain a list of all alias of a given charset.
 * First it discover some needed charset (UTF-8, ISO8859-1 and UCS2) and then
 * try to discover others from those characters (this discover happen only when
 * required).
 *
 * There are a list of canonic names (GNU iconv names) and a set of aliases
 * (one for others iconv implementations and another for Sybase). For every
 * canonic charset name we cache iconv name found during discovery.
 */
void
tds_iconv_open(TDSSOCKET * tds, const char *charset)
{
	static const char *UCS_2LE = "UCS-2LE";
	const char *name;
	int fOK;

	TDS_ENCODING *client = &tds->iconv_info[client2ucs2]->client_charset;
	TDS_ENCODING *server = &tds->iconv_info[client2ucs2]->server_charset;

#if !HAVE_ICONV_ALWAYS

	strcpy(client->name, "ISO-8859-1");
	strcpy(server->name, UCS_2LE);

	bytes_per_char(client);
	bytes_per_char(server);
	return;
#else
	/* initialize */
	if (!iconv_initialized) {
		if (tds_iconv_init()) {
			assert(0);
			return;
		}
		iconv_initialized = 1;
	}

	/* 
	 * Client <-> UCS-2 (client2ucs2)
	 */
	tdsdump_log(TDS_DBG_FUNC, "iconv to convert client-side data to the \"%s\" character set\n", charset);

	fOK = tds_iconv_info_init(tds->iconv_info[client2ucs2], charset, UCS_2LE);
	if (!fOK)
		return;

	/* 
	 * How many UTF-8 bytes we need is a function of what the input character set is.
	 * TODO This could definitely be more sophisticated, but it deals with the common case.
	 */
	if (client->min_bytes_per_char == 1 && client->max_bytes_per_char == 4 && server->max_bytes_per_char == 1) {
		/* ie client is UTF-8 and server is ISO-8859-1 or variant. */
		client->max_bytes_per_char = 3;
	}

	/* 
	 * Client <-> server single-byte charset
	 * TODO: the server hasn't reported its charset yet, so this logic can't work here.  
	 *       not sure what to do about that yet.  
	 */
	if (tds->env && tds->env->charset) {
		fOK = tds_iconv_info_init(tds->iconv_info[client2server_chardata], charset, tds->env->charset);
		if (!fOK)
			return;
	}

	/* 
	 * ISO8859-1 <-> server meta data
	 */
	name = UCS_2LE;
	if (tds->major_version < 7) {
		name = "ISO-8859-1";
		if (tds->env && tds->env->charset)
			name = tds->env->charset;
	}
	fOK = tds_iconv_info_init(tds->iconv_info[iso2server_metadata], "ISO-8859-1", name);
#endif
}

/**
 * Open iconv descriptors to convert between character sets (both directions).
 * 1.  Look up the canonical names of the character sets.
 * 2.  Look up their widths.
 * 3.  Ask iconv to open a conversion descriptor.
 * 4.  Fail if any of the above offer any resistance.  
 * \remarks The charset names written to \a iconv_info will be the canonical names, 
 *          not necessarily the names passed in. 
 */
static int
tds_iconv_info_init(TDSICONVINFO * iconv_info, const char *client_name, const char *server_name)
{
	TDS_ENCODING *client = &iconv_info->client_charset;
	TDS_ENCODING *server = &iconv_info->server_charset;

	int server_canonical, client_canonical;

	assert(client_name && server_name);

	assert(iconv_info->to_wire == (iconv_t) - 1);
	assert(iconv_info->to_wire2 == (iconv_t) - 1);
	assert(iconv_info->from_wire == (iconv_t) - 1);
	assert(iconv_info->from_wire2 == (iconv_t) - 1);

	client_canonical = tds_canonical_charset(client_name);
	server_canonical = tds_canonical_charset(server_name);

	if (client_canonical < 0) {
		tdsdump_log(TDS_DBG_FUNC, "%L tds_iconv_info_init: client charset name \"%s\" unrecognized\n", client->name);
		return 0;
	}

	if (server_canonical < 0) {
		tdsdump_log(TDS_DBG_FUNC, "%L tds_iconv_info_init: server charset name \"%s\" unrecognized\n", client->name);
		return 0;
	}

	*client = canonic_charsets[client_canonical];
	*server = canonic_charsets[server_canonical];

	/* special case, same charset, no conversion */
	if (client_canonical == server_canonical) {
		iconv_info->to_wire = (iconv_t) - 1;
		iconv_info->from_wire = (iconv_t) - 1;
		iconv_info->flags = TDS_ENCODING_MEMCPY;
		return 1;
	}

	iconv_info->flags = 0;
	if (!iconv_names[server_canonical]) {
		switch (server_canonical) {
		case POS_UCS2LE:
			server_canonical = POS_UCS2BE;
			iconv_info->flags = TDS_ENCODING_SWAPBYTE;
			break;
		case POS_UCS2BE:
			server_canonical = POS_UCS2LE;
			iconv_info->flags = TDS_ENCODING_SWAPBYTE;
			break;
		}
	}

	/* get iconv names */
	if (!iconv_names[client_canonical])
		tds_get_iconv_name(client_canonical);
	if (!iconv_names[server_canonical])
		tds_get_iconv_name(server_canonical);

	/* names available ?? */
	if (!iconv_names[client_canonical][0] || !iconv_names[server_canonical][0]) {
		iconv_info->to_wire = (iconv_t) - 1;
		iconv_info->from_wire = (iconv_t) - 1;
		iconv_info->flags = TDS_ENCODING_MEMCPY;
		tdsdump_log(TDS_DBG_FUNC, "%L tds_iconv_info_init: use memcpy to convert \"%s\"->\"%s\"\n", client->name,
			    server->name);
		return 0;
	}

	iconv_info->to_wire = iconv_open(iconv_names[server_canonical], iconv_names[client_canonical]);
	if (iconv_info->to_wire == (iconv_t) - 1) {
		tdsdump_log(TDS_DBG_FUNC, "%L tds_iconv_info_init: cannot convert \"%s\"->\"%s\"\n", client->name, server->name);
	}

	iconv_info->from_wire = iconv_open(iconv_names[client_canonical], iconv_names[server_canonical]);
	if (iconv_info->from_wire == (iconv_t) - 1) {
		tdsdump_log(TDS_DBG_FUNC, "%L tds_iconv_info_init: cannot convert \"%s\"->\"%s\"\n", server->name, client->name);
	}

	/* try indirect conversions */
	if (iconv_info->to_wire == (iconv_t) - 1 || iconv_info->from_wire == (iconv_t) - 1) {
		tds_iconv_info_close(iconv_info);

		/* TODO reuse some conversion, client charset is usually constant in all connection (or ISO8859-1) */
		iconv_info->to_wire = iconv_open(iconv_names[POS_UTF8], iconv_names[client_canonical]);
		iconv_info->to_wire2 = iconv_open(iconv_names[server_canonical], iconv_names[POS_UTF8]);
		iconv_info->from_wire = iconv_open(iconv_names[POS_UTF8], iconv_names[server_canonical]);
		iconv_info->from_wire2 = iconv_open(iconv_names[client_canonical], iconv_names[POS_UTF8]);

		if (iconv_info->to_wire == (iconv_t) - 1 || iconv_info->to_wire2 == (iconv_t) - 1
		    || iconv_info->from_wire == (iconv_t) - 1 || iconv_info->from_wire2 == (iconv_t) - 1) {

			tds_iconv_info_close(iconv_info);
			tdsdump_log(TDS_DBG_FUNC, "%L tds_iconv_info_init: cannot convert \"%s\"->\"%s\" indirectly\n",
				    server->name, client->name);
			return 0;
		}

		iconv_info->flags |= TDS_ENCODING_INDIRECT;
	}
	
	/* TODO, do some optimizations like UCS2 -> UTF8 min,max = 2,2 (UCS2) and 1,4 (UTF8) */

	tdsdump_log(TDS_DBG_FUNC, "%L tds_iconv_info_init: converting \"%s\"->\"%s\"\n", client->name, server->name);

	return 1;
}


#if HAVE_ICONV_ALWAYS
static void
_iconv_close(iconv_t * cd)
{
	static const iconv_t invalid = (iconv_t) - 1;

	if (*cd != invalid) {
		iconv_close(*cd);
		*cd = invalid;
	}
}

static void
tds_iconv_info_close(TDSICONVINFO * iconv_info)
{
	_iconv_close(&iconv_info->to_wire);
	_iconv_close(&iconv_info->to_wire2);
	_iconv_close(&iconv_info->from_wire);
	_iconv_close(&iconv_info->from_wire2);
}
#endif

void
tds_iconv_close(TDSSOCKET * tds)
{
#if HAVE_ICONV_ALWAYS
	int i;

	for (i = 0; i < tds->iconv_info_count; ++i) {
		tds_iconv_info_close(tds->iconv_info[i]);
	}
#endif
}

#define CHUNK_ALLOC 4

void
tds_iconv_free(TDSSOCKET * tds)
{
	int i;

	if (!tds->iconv_info)
		return;
	tds_iconv_close(tds);

	free(tds->iconv_info[0]);
	for (i = initial_iconv_info_count + 1; i < tds->iconv_info_count; i += CHUNK_ALLOC)
		free(tds->iconv_info[i]);
	free(tds->iconv_info);
	tds->iconv_info = NULL;
	tds->iconv_info_count = 0;
}

/** 
 * Wrapper around iconv(3).  Same parameters, with slightly different behavior.
 * \param io Enumerated value indicating whether the data are being sent to or received from the server. 
 * \param iconv_info information about the encodings involved, including the iconv(3) conversion descriptors. 
 * \param inbuf address of pointer to the input buffer of data to be converted.  
 * \param inbytesleft address of count of bytes in \a inbuf.
 * \param outbuf address of pointer to the output buffer.  
 * \param outbytesleft address of count of bytes in \a outbuf.
 * \retval number of irreversible conversions performed.  \i -1 on error, see iconv(3) documentation for 
 * a description of the possible values of \i errno.  
 * \remarks Unlike iconv(3), none of the arguments can be nor point to NULL.  Like iconv(3), all pointers will 
 *  	be updated.  Succcess is signified by a nonnegative return code and \a *inbytesleft == 0.  
 * 	If the conversion descriptor in \a iconv_info is -1 or NULL, \a inbuf is copied to \a outbuf, 
 *	and all parameters updated accordingly. 
 * 
 * 	In the event that a character in \a inbuf cannot be converted because no such cbaracter exists in the
 * 	\a outbuf character set, we emit messages similar to the ones Sybase emits when it fails such a conversion. 
 * 	The message varies depending on the direction of the data.  
 * 	On a read error, we emit Msg 2403, Severity 16 (EX_INFO):
 * 		"WARNING! Some character(s) could not be converted into client's character set. 
 *			Unconverted bytes were changed to question marks ('?')."
 * 	On a write error we emit Msg 2402, Severity 16 (EX_USER):
 *		"Error converting client characters into server's character set. Some character(s) could not be converted."
 *  	  and return an error code.  Client libraries relying on this routine should reflect an error back to the appliction.  
 * 	
 * \todo Check for variable multibyte non-UTF-8 input character set.  
 * \todo Use more robust error message generation.  
 * \todo For reads, cope with \outbuf encodings that don't have the equivalent of an ASCII '?'.  
 * \todo Support alternative to '?' for the replacement character.  
 */
size_t
tds_iconv(TDSSOCKET * tds, const TDSICONVINFO * iconv_info, TDS_ICONV_DIRECTION io,
	  const char **inbuf, size_t * inbytesleft, char **outbuf, size_t * outbytesleft)
{
	static const iconv_t invalid = (iconv_t) - 1;
	const TDS_ENCODING *input_charset = NULL;
	const char *output_charset_name = NULL;

	iconv_t cd = invalid, cd2 = invalid;
	iconv_t error_cd = invalid;

	char quest_mark[] = "?";	/* best to leave non-const; implementations vary */
	ICONV_CONST char *pquest_mark = quest_mark;
	size_t lquest_mark;
	size_t irreversible;
	char one_character;
	char *p;
	int eilseq_raised = 0;
	/* cast away const-ness */
	TDS_ERRNO_MESSAGE_FLAGS *suppress = (TDS_ERRNO_MESSAGE_FLAGS*) &iconv_info->suppress;

	assert(inbuf && inbytesleft && outbuf && outbytesleft);

	switch (io) {
	case to_server:
		cd = iconv_info->to_wire;
		cd2 = iconv_info->to_wire2;
		input_charset = &iconv_info->client_charset;
		output_charset_name = iconv_info->server_charset.name;
		break;
	case to_client:
		cd = iconv_info->from_wire;
		cd2 = iconv_info->from_wire2;
		input_charset = &iconv_info->server_charset;
		output_charset_name = iconv_info->client_charset.name;
		break;
	default:
		tdsdump_log(TDS_DBG_FUNC, "tds_iconv: unable to determine if %d means in or out.  \n", io);
		assert(io == to_server || io == to_client);
		break;
	}

	/* silly case, memcpy */
	if (iconv_info->flags & TDS_ENCODING_MEMCPY || cd == invalid) {
		size_t len = *inbytesleft < *outbytesleft ? *inbytesleft : *outbytesleft;

		memcpy(*outbuf, *inbuf, len);
		errno = *inbytesleft > *outbytesleft ? E2BIG : 0;
		*inbytesleft -= len;
		*outbytesleft -= len;
		*inbuf += len;
		*outbuf += len;
		return 0;
	}

	/*
	 * Call iconv() as many times as necessary, until we reach the end of input or exhaust output.  
	 */
	errno = 0;
	p = *outbuf;
	for (;;) {
		if (iconv_info->flags & TDS_ENCODING_INDIRECT) {
#if ENABLE_EXTRA_CHECKS
			char tmp[8];
#else
			char tmp[128];
#endif
			char *pb = tmp;
			size_t l = sizeof(tmp);
			int temp_errno;
			size_t temp_irreversible;

			temp_irreversible = iconv(cd, (ICONV_CONST char **) inbuf, inbytesleft, &pb, &l);
			temp_errno = errno;

			/* convert partial */
			pb = tmp;
			l = sizeof(tmp) - l;
			for (;;) {
				errno = 0;
				irreversible = iconv(cd2, (ICONV_CONST char **) &pb, &l, outbuf, outbytesleft);
				if (irreversible != (size_t) - 1) {
					if (*inbytesleft)
						break;
					goto end_loop;
				}
				/* EINVAL should be impossible, all characters came from previous iconv... */
				if (errno == E2BIG || errno == EINVAL)
					goto end_loop;

				/*
				 * error should be EILSEQ, not convertible sequence 
				 * skip UTF-8 sequence 
				 */
				/* avoid infinite recursion */
				eilseq_raised = 1;
				if (*pb == '?')
					goto end_loop;
				*pb = 0x80;
				while(l && (*pb & 0xC0) == 0x80)
					++pb, --l;
				--pb;
				++l;
				*pb = '?';
			}
			if (temp_errno == E2BIG) {
				errno = 0;
				continue;
			}
			errno = temp_errno;
			irreversible = temp_irreversible;
			break;
		} else if (io == to_client && iconv_info->flags & TDS_ENCODING_SWAPBYTE) {
			/* swap bytes if necessary */
#if ENABLE_EXTRA_CHECKS
			char tmp[8];
#else
			char tmp[128];
#endif
			char *pib = tmp;
			size_t il = *inbytesleft > sizeof(tmp) ? sizeof(tmp) : *inbytesleft;
			size_t n;

			for (n = 0; n < il; n += 2) {
				tmp[n] = (*inbuf)[n + 1];
				tmp[n + 1] = (*inbuf)[n];
			}
			irreversible = iconv(cd, (ICONV_CONST char **) &pib, &il, outbuf, outbytesleft);
			il = pib - tmp;
			*inbuf += il;
			*inbytesleft -= il;
			if (irreversible != (size_t) - 1 && *inbytesleft)
				continue;
		} else {
			irreversible = iconv(cd, (ICONV_CONST char **) inbuf, inbytesleft, outbuf, outbytesleft);
		}
		if (irreversible != (size_t) - 1)
			break;

		if (errno == EILSEQ)
			eilseq_raised = 1;

		if (errno != EILSEQ || io != to_client)
			break;
		/* 
		 * Invalid input sequence encountered reading from server. 
		 * Skip one input sequence, adjusting pointers. 
		 */
		one_character = skip_one_input_sequence(cd, input_charset, inbuf, inbytesleft);

		if (!one_character)
			break;

		/* 
		 * To replace invalid input with '?', we have to convert a UTF-8 '?' into the output character set.  
		 * In unimaginably weird circumstances, this might be impossible.
		 * We use UTF-8 instead of ASCII because some implementations 
		 * do not convert singlebyte <-> singlebyte.
		 */
		if (error_cd == invalid) {
			error_cd = iconv_open(output_charset_name, iconv_names[POS_UTF8]);
			if (error_cd == invalid) {
				break;	/* what to do? */
			}
		}

		lquest_mark = 1;
		pquest_mark = quest_mark;

		p = *outbuf;
		irreversible = iconv(error_cd, &pquest_mark, &lquest_mark, outbuf, outbytesleft);

		if (irreversible == (size_t) - 1)
			break;

		if (!*inbytesleft)
			break;
	}
end_loop:
	
	/* swap bytes if necessary */
	if (io == to_server && iconv_info->flags & TDS_ENCODING_SWAPBYTE) {
		assert((*outbuf - p) % 2 == 0);
		for (; p < *outbuf; p += 2) {
			char tmp = p[0];

			p[0] = p[1];
			p[1] = tmp;
		}
	}

	if (eilseq_raised && !suppress->eilseq) {
		/* invalid multibyte input sequence encountered */
		if (io == to_client) {
			if (irreversible == (size_t) - 1) {
				tds_client_msg(tds->tds_ctx, tds, 2404, 16, 0, 0,
					       "WARNING! Some character(s) could not be converted into client's character set. ");
			} else {
				tds_client_msg(tds->tds_ctx, tds, 2403, 16, 0, 0,
					       "WARNING! Some character(s) could not be converted into client's character set. "
					       "Unconverted bytes were changed to question marks ('?').");
				errno = 0;
			}
		} else {
			tds_client_msg(tds->tds_ctx, tds, 2402, 16, 0, 0,
				       "Error converting client characters into server's character set. "
				       "Some character(s) could not be converted.");
		}
		suppress->eilseq = 1;
	}

	switch (errno) {
	case EINVAL:		/* incomplete multibyte sequence is encountered */
		if (suppress->einval)
			break;
		/* FIXME in chunk conversion this can mean we end a chunk inside a character */
		tds_client_msg(tds->tds_ctx, tds, 2401, 16, *inbytesleft, 0,
			       "iconv EINVAL: Error converting between character sets. "
			       "Conversion abandoned at offset indicated by the \"state\" value of this message.");
		suppress->einval = 1;
		break;
	case E2BIG:		/* output buffer has no more room */
		if (suppress->e2big)
			break;
		tds_client_msg(tds->tds_ctx, tds, 2400, 16, *inbytesleft, 0,
			       "iconv E2BIG: Error converting between character sets. " "Output buffer exhausted.");
		suppress->e2big = 1;
		break;
	default:
		break;
	}

	if (error_cd != invalid) {
		iconv_close(error_cd);
	}

	return irreversible;
}

/**
 * Read a data file, passing the data through iconv().
 * \return Count of bytes either not read, or read but not converted.  Returns zero on success.  
 */
size_t
tds_iconv_fread(iconv_t cd, FILE * stream, size_t field_len, size_t term_len, char *outbuf, size_t * outbytesleft)
{
	char buffer[16000];
	char *ib;
	size_t isize = 0, nonreversible_conversions = 0;

	/*
	 * If cd isn't valid, it's just an indication that this column needs no conversion.  
	 */
	if (cd == (iconv_t) - 1 || cd == NULL) {
		assert(field_len <= *outbytesleft);
		if (field_len > 0) {
			if (1 != fread(outbuf, field_len, 1, stream)) {
				return field_len + term_len;	/* unable to read */
			}
		}

		/* prepare to read the terminator and return */
		*outbytesleft -= field_len;	/* as iconv would have done */
		isize = 0;			/* as iconv would have done */
		field_len = 0;			/* as the loop would have done */

		goto READ_TERMINATOR;
	}
	
	/*
	 * Read in chunks.  
	 * 	field_len  is the total size to read
	 * 	isize	   is the size of the current chunk (which might be the whole thing).
	 * They are decremented as they are successfully processed.  
	 * On success, we exit the loop with both equal to zero, indicating nothing we
	 * were asked to read remains unread.
	 */
	isize = (sizeof(buffer) < field_len) ? sizeof(buffer) : field_len;

	for (ib = buffer; isize && 1 == fread(ib, isize, 1, stream);) {

		tdsdump_log(TDS_DBG_FUNC, "%L tds_iconv_fread: read %d of %d bytes; outbuf has %d left.\n", isize, field_len,
			    *outbytesleft);
		field_len -= isize;

		nonreversible_conversions += iconv(cd, (ICONV_CONST char **) &ib, &isize, &outbuf, outbytesleft);

		if (isize != 0) {
			switch (errno) {
			case EINVAL:	/* incomplete multibyte sequence encountered in input */
				memmove(buffer, buffer + sizeof(buffer) - isize, isize);
				ib = buffer + isize;
				isize = sizeof(buffer) - isize;
				if (isize < field_len)
					isize = field_len;
				continue;
			case E2BIG:	/* insufficient room in output buffer */
			case EILSEQ:	/* invalid multibyte sequence encountered in input */
			default:
				/* FIXME: emit message */
				tdsdump_log(TDS_DBG_FUNC, "%L tds_iconv_fread: error %d: %s.\n", errno, strerror(errno));
				break;
			}
		}
		isize = (sizeof(buffer) < field_len) ? sizeof(buffer) : field_len;
	}
	
	READ_TERMINATOR:

	if (term_len > 0 && !feof(stream)) {
		isize += term_len;
		if (term_len && 1 == fread(buffer, term_len, 1, stream)) {
			isize -= term_len;
		} else {
			tdsdump_log(TDS_DBG_FUNC, "%L tds_iconv_fread: cannot read %d-byte terminator\n", term_len);
		}
	}

	return field_len + isize;
}

/**
 * Get a iconv info structure, allocate and initialize if needed
 */
static TDSICONVINFO *
tds_iconv_get_info(TDSSOCKET * tds, const char *canonic_charset)
{
	TDSICONVINFO *info;
	int i;

	/* search a charset from already allocated charsets */
	for (i = tds->iconv_info_count; --i >= initial_iconv_info_count;)
		if (strcmp(canonic_charset, tds->iconv_info[i]->server_charset.name) == 0)
			return tds->iconv_info[i];

	/* allocate a new iconv structure */
	if (tds->iconv_info_count % CHUNK_ALLOC == ((initial_iconv_info_count + 1) % CHUNK_ALLOC)) {
		TDSICONVINFO **p;
		TDSICONVINFO *infos;

		infos = (TDSICONVINFO *) malloc(sizeof(TDSICONVINFO) * CHUNK_ALLOC);
		if (!infos)
			return NULL;
		p = (TDSICONVINFO **) realloc(tds->iconv_info, sizeof(TDSICONVINFO *) * (tds->iconv_info_count + CHUNK_ALLOC));
		if (!p) {
			free(infos);
			return NULL;
		}
		tds->iconv_info = p;
		memset(infos, 0, sizeof(TDSICONVINFO) * CHUNK_ALLOC);
		for (i = 0; i < CHUNK_ALLOC; ++i) {
			tds->iconv_info[i + tds->iconv_info_count] = &infos[i];
			infos[i].server_charset.name = infos[i].client_charset.name = "";
			infos[i].to_wire = (iconv_t) - 1;
			infos[i].to_wire2 = (iconv_t) - 1;
			infos[i].from_wire = (iconv_t) - 1;
			infos[i].from_wire2 = (iconv_t) - 1;
		}
	}
	info = tds->iconv_info[tds->iconv_info_count++];

	/* init */
	/* TODO test allocation */
	tds_iconv_info_init(info, tds->iconv_info[client2ucs2]->client_charset.name, canonic_charset);
	return info;
}

/* change singlebyte conversions according to server */
void
tds_srv_charset_changed(TDSSOCKET * tds, const char *charset)
{
#if HAVE_ICONV_ALWAYS
	TDSICONVINFO *iconv_info = tds->iconv_info[client2server_chardata];
	const char *canonic_charset = tds_canonical_charset_name(charset);

	if (strcmp(canonic_charset, iconv_info->server_charset.name) == 0)
		return;

	/* find and set conversion */
	iconv_info = tds_iconv_get_info(tds, canonic_charset);
	if (iconv_info)
		tds->iconv_info[client2server_chardata] = iconv_info;

	/* if sybase change also server conversions */
	if (tds->major_version >= 7)
		return;

	iconv_info = tds->iconv_info[iso2server_metadata];

	tds_iconv_info_close(iconv_info);

	tds_iconv_info_init(iconv_info, "ISO-8859-1", charset);
#endif
}

/* change singlebyte conversions according to server */
void
tds7_srv_charset_changed(TDSSOCKET * tds, int lcid)
{
	tds_srv_charset_changed(tds, lcid2charset(lcid));
}

#if !HAVE_ICONV_ALWAYS
/**
 * Determine byte/char for an iconv character set.  
 * \retval 0 failed, no such charset.
 * \retval 1 succeeded, fixed byte/char.
 * \retval 2 succeeded, variable byte/char.
 */
static int
bytes_per_char(TDS_ENCODING * charset)
{
	int i;

	assert(charset && strlen(charset->name) < sizeof(charset->name));

	for (i = 0; i < sizeof(canonic_charsets) / sizeof(TDS_ENCODING); i++) {
		if (canonic_charsets[i].min_bytes_per_char == 0)
			break;

		if (0 == strcmp(charset->name, canonic_charsets[i].name)) {
			charset->min_bytes_per_char = canonic_charsets[i].min_bytes_per_char;
			charset->max_bytes_per_char = canonic_charsets[i].max_bytes_per_char;

			return (charset->max_bytes_per_char == charset->min_bytes_per_char) ? 1 : 2;
		}
	}

	return 0;
}
#endif

/**
 * Move the input sequence pointer to the next valid position.
 * Used when an input character cannot be converted.  
 * \returns number of bytes to skip.
 */
/* FIXME possible buffer reading overflow ?? */
static int
skip_one_input_sequence(iconv_t cd, const TDS_ENCODING * charset, const char **input, size_t * input_size)
{
	int charsize = CHARSIZE(charset);
	char ib[16];
	char ob[16];
	ICONV_CONST char *pib;
	char *pob;
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
		*input_size -= charsize;
		return charsize;
	}

	/* handle state encoding */

	/* extract state from iconv */
	pob = ib;
	ol = sizeof(ib);
	iconv(cd, NULL, NULL, &pob, &ol);

	/* init destination conversion */
	/* TODO use largest fixed size for this platform */
	cd2 = iconv_open("UCS-4", charset->name);
	if (cd2 == (iconv_t) - 1)
		return 0;

	/* add part of input */
	il = ol;
	if (il > *input_size)
		il = *input_size;
	l = sizeof(ib) - ol;
	memcpy(ib + l, *input, il);
	il += l;

	/* translate a single character */
	pib = ib;
	pob = ob;
	/* TODO use size of largest fixed charset */
	ol = 4;
	iconv(cd2, &pib, &il, &pob, &ol);

	/* adjust input */
	l = (pib - ib) - l;
	*input += l;
	*input_size -= l;

	/* extract state */
	pob = ib;
	ol = sizeof(ib);
	iconv(cd, NULL, NULL, &pob, &ol);

	/* set input state */
	pib = ib;
	il = sizeof(ib) - ol;
	pob = ob;
	ol = sizeof(ob);
	iconv(cd, &pib, &il, &pob, &ol);

	iconv_close(cd2);

	return l;
}

static int
lookup_canonic(const CHARACTER_SET_ALIAS aliases[], const char *charset_name)
{
	int i;

	for (i = 0; aliases[i].alias; ++i) {
		if (0 == strcmp(charset_name, aliases[i].alias))
			return aliases[i].canonic;
	}

	return -1;
}

/**
 * Determine canonical iconv character set.
 * \returns canonical position, or -1 if lookup failed.
 * \remarks Returned name can be used in bytes_per_char(), above.
 */
static int
tds_canonical_charset(const char *charset_name)
{
	int res;

	/* search in alternative */
	res = lookup_canonic(iconv_aliases, charset_name);
	if (res >= 0)
		return res;

	/* search in sybase */
	return lookup_canonic(sybase_aliases, charset_name);
}

/**
 * Determine canonical iconv character set name.  
 * \returns canonical name, or NULL if lookup failed.
 * \remarks Returned name can be used in bytes_per_char(), above.
 */
const char *
tds_canonical_charset_name(const char *charset_name)
{
	int res;

	/* get numeric pos */
	res = tds_canonical_charset(charset_name);
	if (res >= 0)
		return canonic_charsets[res].name;

	return NULL;
}

/**
 * Determine the name Sybase uses for a character set, given a canonical iconv name.  
 * \returns Sybase name, or NULL if lookup failed.
 * \remarks Returned name can be sent to Sybase a server.
 */
const char *
tds_sybase_charset_name(const char *charset_name)
{
	int res, i;

	/* search in sybase */
	res = lookup_canonic(iconv_aliases, charset_name);
	if (res < 0)
		return NULL;

	/* special case, ignore ascii_8, take iso_1 instead, note index start from 1 */
	assert(strcmp(sybase_aliases[0].alias, "ascii_8") == 0);

	for (i = 1; sybase_aliases[i].alias; ++i) {
		if (sybase_aliases[i].canonic == res)
			return sybase_aliases[i].alias;
	}

	return NULL;
}

static const char *
lcid2charset(int lcid)
{
	/* The table from the MSQLServer reference "Windows Collation Designators" 
	 * and from " NLS Information for Microsoft Windows XP"
	 */

	const char *cp = NULL;

	switch (lcid & 0xffff) {
	case 0x405:
	case 0x40e:		/* 0x1040e */
	case 0x415:
	case 0x418:
	case 0x41a:
	case 0x41b:
	case 0x41c:
	case 0x424:
		/* case 0x81a: seem wrong in XP table TODO check */
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
	case 0x407:		/* 0x10407 */
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
	case 0x437:		/* 0x10437 */
	case 0x438:
		/*case 0x439:  ??? Unicode only */
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
	case 0x411:		/* 0x10411 */
		cp = "CP932";
		break;
	case 0x1004:
	case 0x804:		/* 0x20804 */
		cp = "CP936";
		break;
	case 0x412:		/* 0x10412 */
		cp = "CP949";
		break;
	case 0x1404:
	case 0x404:		/* 0x30404 */
	case 0xc04:
		cp = "CP950";
		break;
	default:
		cp = "CP1252";
	}

	assert(cp);
	return cp;
}

/**
 * Get iconv information from a LCID (to support different column encoding under MSSQL2K)
 */
TDSICONVINFO *
tds_iconv_from_lcid(TDSSOCKET * tds, int lcid)
{
	const char *charset = lcid2charset(lcid);

#if ENABLE_EXTRA_CHECKS
	assert(strcmp(tds_canonical_charset_name(charset), charset) == 0);
#endif

	/* same as client (usually this is true, so this improve performance) ? */
	if (strcmp(tds->iconv_info[client2server_chardata]->server_charset.name, charset) == 0)
		return tds->iconv_info[client2server_chardata];

	return tds_iconv_get_info(tds, charset);
}

/** \@} */
