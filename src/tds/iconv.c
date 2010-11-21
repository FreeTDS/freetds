/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
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

/*
 * iconv.c, handle all the conversion stuff without spreading #if HAVE_ICONV_ALWAYS 
 * all over the other code
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

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

TDS_RCSID(var, "$Id: iconv.c,v 1.146 2010-11-21 21:24:09 freddy77 Exp $");

#define CHARSIZE(charset) ( ((charset)->min_bytes_per_char == (charset)->max_bytes_per_char )? \
				(charset)->min_bytes_per_char : 0 )


#if !HAVE_ICONV_ALWAYS
static int bytes_per_char(TDS_ENCODING * charset);
#endif
static const char *collate2charset(int sql_collate, int lcid);
static size_t skip_one_input_sequence(iconv_t cd, const TDS_ENCODING * charset, const char **input, size_t * input_size);
static int tds_iconv_info_init(TDSICONV * char_conv, const char *client_name, const char *server_name);
static int tds_iconv_init(void);
static int tds_canonical_charset(const char *charset_name);
static void _iconv_close(iconv_t * cd);
static void tds_iconv_info_close(TDSICONV * char_conv);


/**
 * \ingroup libtds
 * \defgroup conv Charset conversion
 * Convert between different charsets.
 */


#include "encodings.h"

/* this will contain real iconv names */
static const char *iconv_names[sizeof(canonic_charsets) / sizeof(canonic_charsets[0])];
static int iconv_initialized = 0;
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
	cd = tds_sys_iconv_open("ISO-8859-1", "UTF-8");
	if (cd != (iconv_t) -1) {
		iconv_names[POS_ISO1] = "ISO-8859-1";
		iconv_names[POS_UTF8] = "UTF-8";
		tds_sys_iconv_close(cd);
	} else {

		/* search names for ISO8859-1 and UTF-8 */
		for (i = 0; iconv_aliases[i].alias; ++i) {
			int j;

			if (iconv_aliases[i].canonic != POS_ISO1)
				continue;
			for (j = 0; iconv_aliases[j].alias; ++j) {
				if (iconv_aliases[j].canonic != POS_UTF8)
					continue;

				cd = tds_sys_iconv_open(iconv_aliases[i].alias, iconv_aliases[j].alias);
				if (cd != (iconv_t) -1) {
					iconv_names[POS_ISO1] = iconv_aliases[i].alias;
					iconv_names[POS_UTF8] = iconv_aliases[j].alias;
					tds_sys_iconv_close(cd);
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
	cd = tds_sys_iconv_open(iconv_names[POS_ISO1], "UCS-2LE");
	if (cd != (iconv_t) -1) {
		iconv_names[POS_UCS2LE] = "UCS-2LE";
		tds_sys_iconv_close(cd);
	}
	cd = tds_sys_iconv_open(iconv_names[POS_ISO1], "UCS-2BE");
	if (cd != (iconv_t) -1) {
		iconv_names[POS_UCS2BE] = "UCS-2BE";
		tds_sys_iconv_close(cd);
	}

	/* long search needed ?? */
	if (!iconv_names[POS_UCS2LE] || !iconv_names[POS_UCS2BE]) {
		for (i = 0; iconv_aliases[i].alias; ++i) {
			if (strncmp(canonic_charsets[iconv_aliases[i].canonic].name, "UCS-2", 5) != 0)
				continue;

			cd = tds_sys_iconv_open(iconv_aliases[i].alias, iconv_names[POS_ISO1]);
			if (cd != (iconv_t) -1) {
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
				if (tds_sys_iconv(cd, &pib, &il, &pob, &ol) != (size_t) - 1) {
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
				tds_sys_iconv_close(cd);
			}
		}
	}
	/* we need a UCS-2 (big endian or little endian) */
	if (!iconv_names[POS_UCS2LE] && !iconv_names[POS_UCS2BE])
		return 2;

	ucs2name = iconv_names[POS_UCS2LE] ? iconv_names[POS_UCS2LE] : iconv_names[POS_UCS2BE];

	for (i = 0; i < 4; ++i)
		tdsdump_log(TDS_DBG_INFO1, "local name for %s is %s\n", canonic_charsets[i].name,
			    iconv_names[i] ? iconv_names[i] : "(null)");

	/* success (it should always occurs) */
	return 0;
}

/**
 * Get iconv name given canonic
 */
static const char *
tds_set_iconv_name(int charset)
{
	int i;
	iconv_t cd;

	assert(iconv_initialized);

	/* try using canonic name and UTF-8 and UCS2 */
	cd = tds_sys_iconv_open(iconv_names[POS_UTF8], canonic_charsets[charset].name);
	if (cd != (iconv_t) -1) {
		iconv_names[charset] = canonic_charsets[charset].name;
		tds_sys_iconv_close(cd);
		return iconv_names[charset];
	}
	cd = tds_sys_iconv_open(ucs2name, canonic_charsets[charset].name);
	if (cd != (iconv_t) -1) {
		iconv_names[charset] = canonic_charsets[charset].name;
		tds_sys_iconv_close(cd);
		return iconv_names[charset];
	}

	/* try all alternatives */
	for (i = 0; iconv_aliases[i].alias; ++i) {
		if (iconv_aliases[i].canonic != charset)
			continue;

		cd = tds_sys_iconv_open(iconv_names[POS_UTF8], iconv_aliases[i].alias);
		if (cd != (iconv_t) -1) {
			iconv_names[charset] = iconv_aliases[i].alias;
			tds_sys_iconv_close(cd);
			return iconv_names[charset];
		}

		cd = tds_sys_iconv_open(ucs2name, iconv_aliases[i].alias);
		if (cd != (iconv_t) -1) {
			iconv_names[charset] = iconv_aliases[i].alias;
			tds_sys_iconv_close(cd);
			return iconv_names[charset];
		}
	}

	/* charset not found, pretend it's ISO 8859-1 */
	iconv_names[charset] = canonic_charsets[POS_ISO1].name;
	return NULL;
}

static void
tds_iconv_reset(TDSICONV *conv)
{
	/*
	 * (min|max)_bytes_per_char can be used to divide
	 * so init to safe values
	 */
	conv->server_charset.min_bytes_per_char = 1;
	conv->server_charset.max_bytes_per_char = 1;
	conv->client_charset.min_bytes_per_char = 1;
	conv->client_charset.max_bytes_per_char = 1;

	conv->server_charset.name = conv->client_charset.name = "";
	conv->to_wire = (iconv_t) -1;
	conv->to_wire2 = (iconv_t) -1;
	conv->from_wire = (iconv_t) -1;
	conv->from_wire2 = (iconv_t) -1;
}

/**
 * Allocate iconv stuff
 * \return 0 for success
 */
int
tds_iconv_alloc(TDSSOCKET * tds)
{
	int i;
	TDSICONV *char_conv;

	assert(!tds->char_convs);
	if (!(tds->char_convs = (TDSICONV **) malloc(sizeof(TDSICONV *) * (initial_char_conv_count + 1))))
		return 1;
	char_conv = (TDSICONV *) calloc(initial_char_conv_count, sizeof(TDSICONV));
	if (!char_conv) {
		TDS_ZERO_FREE(tds->char_convs);
		return 1;
	}
	tds->char_conv_count = initial_char_conv_count + 1;

	for (i = 0; i < initial_char_conv_count; ++i) {
		tds->char_convs[i] = &char_conv[i];
		tds_iconv_reset(&char_conv[i]);
	}

	/* chardata is just a pointer to another iconv info */
	tds->char_convs[initial_char_conv_count] = tds->char_convs[client2server_chardata];

	return 0;
}

/**
 * \addtogroup conv
 * @{ 
 * Set up the initial iconv conversion descriptors.
 * When the socket is allocated, three TDSICONV structures are attached to iconv.  
 * They have fixed meanings:
 * 	\li 0. Client <-> UCS-2 (client2ucs2)
 * 	\li 1. Client <-> server single-byte charset (client2server_chardata)
 *	\li 2. ISO8859-1  <-> server meta data	(iso2server_metadata)
 *
 * Other designs that use less data are possible, but these three conversion needs are 
 * very often needed.  By reserving them, we avoid searching the array for our most common purposes.
 *
 * To solve different iconv names and portability problems FreeTDS maintains 
 * a list of aliases each charset.  
 * 
 * First we discover the names of our minimum required charsets (UTF-8, ISO8859-1 and UCS2).  
 * Later, as and when it's needed, we try to discover others.
 *
 * There is one list of canonic names (GNU iconv names) and two sets of aliases
 * (one for other iconv implementations and another for Sybase). For every
 * canonic charset name we cache the iconv name found during discovery. 
 */
void
tds_iconv_open(TDSSOCKET * tds, const char *charset)
{
	static const char UCS_2LE[] = "UCS-2LE";
	const char *name;
	int fOK, ret;

	TDS_ENCODING *client = &tds->char_convs[client2ucs2]->client_charset;
	TDS_ENCODING *server = &tds->char_convs[client2ucs2]->server_charset;

	tdsdump_log(TDS_DBG_FUNC, "tds_iconv_open(%p, %s)\n", tds, charset);

#if !HAVE_ICONV_ALWAYS

	strcpy(client->name, "ISO-8859-1");
	strcpy(server->name, UCS_2LE);

	bytes_per_char(client);
	bytes_per_char(server);
	return;
#else
	/* initialize */
	if (!iconv_initialized) {
		if ((ret = tds_iconv_init()) > 0) {
			static const char names[][12] = { "ISO 8859-1", "UTF-8" };
			assert(ret < 3);
			tdsdump_log(TDS_DBG_FUNC, "error: tds_iconv_init() returned %d; "
						  "could not find a name for %s that your iconv accepts.\n"
						  "use: \"configure --disable-libiconv\"", ret, names[ret-1]);
			assert(ret == 0);
			return;
		}
		iconv_initialized = 1;
	}

	/* 
	 * Client <-> UCS-2 (client2ucs2)
	 */
	tdsdump_log(TDS_DBG_FUNC, "setting up conversions for client charset \"%s\"\n", charset);

	tdsdump_log(TDS_DBG_FUNC, "preparing iconv for \"%s\" <-> \"%s\" conversion\n", charset, UCS_2LE);

	fOK = tds_iconv_info_init(tds->char_convs[client2ucs2], charset, UCS_2LE);
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
	tds->char_convs[client2server_chardata]->flags = TDS_ENCODING_MEMCPY;
	if (tds->env.charset) {
		tdsdump_log(TDS_DBG_FUNC, "preparing iconv for \"%s\" <-> \"%s\" conversion\n", charset, tds->env.charset);
		fOK = tds_iconv_info_init(tds->char_convs[client2server_chardata], charset, tds->env.charset);
		if (!fOK)
			return;
	} else {
		int canonic_charset = tds_canonical_charset(charset);
		tds->char_convs[client2server_chardata]->client_charset = canonic_charsets[canonic_charset];
		tds->char_convs[client2server_chardata]->server_charset = canonic_charsets[canonic_charset];
	}

	/* 
	 * ISO8859-1 <-> server meta data
	 */
	name = UCS_2LE;
	if (!IS_TDS7_PLUS(tds)) {
		name = "ISO-8859-1";
		if (tds->env.charset)
			name = tds->env.charset;
	}
	tdsdump_log(TDS_DBG_FUNC, "preparing iconv for \"%s\" <-> \"%s\" conversion\n", "ISO-8859-1", name);
	fOK = tds_iconv_info_init(tds->char_convs[iso2server_metadata], "ISO-8859-1", name);

	tdsdump_log(TDS_DBG_FUNC, "tds_iconv_open: done\n");
#endif
}

/**
 * Open iconv descriptors to convert between character sets (both directions).
 * 1.  Look up the canonical names of the character sets.
 * 2.  Look up their widths.
 * 3.  Ask iconv to open a conversion descriptor.
 * 4.  Fail if any of the above offer any resistance.  
 * \remarks The charset names written to \a iconv will be the canonical names, 
 *          not necessarily the names passed in. 
 */
static int
tds_iconv_info_init(TDSICONV * char_conv, const char *client_name, const char *server_name)
{
	TDS_ENCODING *client = &char_conv->client_charset;
	TDS_ENCODING *server = &char_conv->server_charset;

	int server_canonical, client_canonical;

	assert(client_name && server_name);

	assert(char_conv->to_wire == (iconv_t) -1);
	assert(char_conv->to_wire2 == (iconv_t) -1);
	assert(char_conv->from_wire == (iconv_t) -1);
	assert(char_conv->from_wire2 == (iconv_t) -1);

	client_canonical = tds_canonical_charset(client_name);
	server_canonical = tds_canonical_charset(server_name);

	if (client_canonical < 0) {
		tdsdump_log(TDS_DBG_FUNC, "tds_iconv_info_init: client charset name \"%s\" unrecognized\n", client_name);
		return 0;
	}

	if (server_canonical < 0) {
		tdsdump_log(TDS_DBG_FUNC, "tds_iconv_info_init: server charset name \"%s\" unrecognized\n", server_name);
		return 0;
	}

	*client = canonic_charsets[client_canonical];
	*server = canonic_charsets[server_canonical];

	/* special case, same charset, no conversion */
	if (client_canonical == server_canonical) {
		char_conv->to_wire = (iconv_t) -1;
		char_conv->from_wire = (iconv_t) -1;
		char_conv->flags = TDS_ENCODING_MEMCPY;
		return 1;
	}

	char_conv->flags = 0;
	if (!iconv_names[server_canonical]) {
		switch (server_canonical) {
		case POS_UCS2LE:
			server_canonical = POS_UCS2BE;
			char_conv->flags = TDS_ENCODING_SWAPBYTE;
			break;
		case POS_UCS2BE:
			server_canonical = POS_UCS2LE;
			char_conv->flags = TDS_ENCODING_SWAPBYTE;
			break;
		}
	}

	/* get iconv names */
	if (!iconv_names[client_canonical]) {
		if (!tds_set_iconv_name(client_canonical)) {
			tdsdump_log(TDS_DBG_FUNC, "\"%s\" not supported by iconv, using \"%s\" instead\n",
						  client_name, iconv_names[client_canonical]);
		}
	}
	
	if (!iconv_names[server_canonical]) {
		if (!tds_set_iconv_name(server_canonical)) {
			tdsdump_log(TDS_DBG_FUNC, "\"%s\" not supported by iconv, using \"%s\" instead\n",
						  server_name, iconv_names[server_canonical]);
		}
	}

	char_conv->to_wire = tds_sys_iconv_open(iconv_names[server_canonical], iconv_names[client_canonical]);
	if (char_conv->to_wire == (iconv_t) -1) {
		tdsdump_log(TDS_DBG_FUNC, "tds_iconv_info_init: cannot convert \"%s\"->\"%s\"\n", client->name, server->name);
	}

	char_conv->from_wire = tds_sys_iconv_open(iconv_names[client_canonical], iconv_names[server_canonical]);
	if (char_conv->from_wire == (iconv_t) -1) {
		tdsdump_log(TDS_DBG_FUNC, "tds_iconv_info_init: cannot convert \"%s\"->\"%s\"\n", server->name, client->name);
	}

	/* try indirect conversions */
	if (char_conv->to_wire == (iconv_t) -1 || char_conv->from_wire == (iconv_t) -1) {
		tds_iconv_info_close(char_conv);

		/* TODO reuse some conversion, client charset is usually constant in all connection (or ISO8859-1) */
		char_conv->to_wire = tds_sys_iconv_open(iconv_names[POS_UTF8], iconv_names[client_canonical]);
		char_conv->to_wire2 = tds_sys_iconv_open(iconv_names[server_canonical], iconv_names[POS_UTF8]);
		char_conv->from_wire = tds_sys_iconv_open(iconv_names[POS_UTF8], iconv_names[server_canonical]);
		char_conv->from_wire2 = tds_sys_iconv_open(iconv_names[client_canonical], iconv_names[POS_UTF8]);

		if (char_conv->to_wire == (iconv_t) -1 || char_conv->to_wire2 == (iconv_t) -1
		    || char_conv->from_wire == (iconv_t) -1 || char_conv->from_wire2 == (iconv_t) -1) {

			tds_iconv_info_close(char_conv);
			tdsdump_log(TDS_DBG_FUNC, "tds_iconv_info_init: cannot convert \"%s\"->\"%s\" indirectly\n",
				    server->name, client->name);
			return 0;
		}

		char_conv->flags |= TDS_ENCODING_INDIRECT;
	}
	
	/* TODO, do some optimizations like UCS2 -> UTF8 min,max = 2,2 (UCS2) and 1,4 (UTF8) */

	/* tdsdump_log(TDS_DBG_FUNC, "tds_iconv_info_init: converting \"%s\"->\"%s\"\n", client->name, server->name); */

	return 1;
}


#if HAVE_ICONV_ALWAYS
static void
_iconv_close(iconv_t * cd)
{
	static const iconv_t invalid = (iconv_t) -1;

	if (*cd != invalid) {
		tds_sys_iconv_close(*cd);
		*cd = invalid;
	}
}

static void
tds_iconv_info_close(TDSICONV * char_conv)
{
	_iconv_close(&char_conv->to_wire);
	_iconv_close(&char_conv->to_wire2);
	_iconv_close(&char_conv->from_wire);
	_iconv_close(&char_conv->from_wire2);
}
#endif

void
tds_iconv_close(TDSSOCKET * tds)
{
#if HAVE_ICONV_ALWAYS
	int i;

	for (i = 0; i < tds->char_conv_count; ++i) {
		tds_iconv_info_close(tds->char_convs[i]);
	}
#endif
}

#define CHUNK_ALLOC 4

void
tds_iconv_free(TDSSOCKET * tds)
{
	int i;

	if (!tds->char_convs)
		return;
	tds_iconv_close(tds);

	free(tds->char_convs[0]);
	for (i = initial_char_conv_count + 1; i < tds->char_conv_count; i += CHUNK_ALLOC)
		free(tds->char_convs[i]);
	TDS_ZERO_FREE(tds->char_convs);
	tds->char_conv_count = 0;
}

/** 
 * Wrapper around iconv(3).  Same parameters, with slightly different behavior.
 * \param tds state information for the socket and the TDS protocol
 * \param io Enumerated value indicating whether the data are being sent to or received from the server. 
 * \param conv information about the encodings involved, including the iconv(3) conversion descriptors. 
 * \param inbuf address of pointer to the input buffer of data to be converted.  
 * \param inbytesleft address of count of bytes in \a inbuf.
 * \param outbuf address of pointer to the output buffer.  
 * \param outbytesleft address of count of bytes in \a outbuf.
 * \retval number of irreversible conversions performed.  -1 on error, see iconv(3) documentation for 
 * a description of the possible values of \e errno.  
 * \remarks Unlike iconv(3), none of the arguments can be nor point to NULL.  Like iconv(3), all pointers will 
 *  	be updated.  Success is signified by a nonnegative return code and \a *inbytesleft == 0.  
 * 	If the conversion descriptor in \a iconv is -1 or NULL, \a inbuf is copied to \a outbuf, 
 *	and all parameters updated accordingly. 
 * 
 * 	If a character in \a inbuf cannot be converted because no such cbaracter exists in the
 * 	\a outbuf character set, we emit messages similar to the ones Sybase emits when it fails such a conversion. 
 * 	The message varies depending on the direction of the data.  
 * 	On a read error, we emit Msg 2403, Severity 16 (EX_INFO):
 * 		"WARNING! Some character(s) could not be converted into client's character set. 
 *			Unconverted bytes were changed to question marks ('?')."
 * 	On a write error we emit Msg 2402, Severity 16 (EX_USER):
 *		"Error converting client characters into server's character set. Some character(s) could not be converted."
 *  	  and return an error code.  Client libraries relying on this routine should reflect an error back to the application.  
 * 	
 * \todo Check for variable multibyte non-UTF-8 input character set.  
 * \todo Use more robust error message generation.  
 * \todo For reads, cope with \a outbuf encodings that don't have the equivalent of an ASCII '?'.  
 * \todo Support alternative to '?' for the replacement character.  
 */
size_t
tds_iconv(TDSSOCKET * tds, const TDSICONV * conv, TDS_ICONV_DIRECTION io,
	  const char **inbuf, size_t * inbytesleft, char **outbuf, size_t * outbytesleft)
{
	static const iconv_t invalid = (iconv_t) -1;
	const TDS_ENCODING *input_charset = NULL;
	const char *output_charset_name = NULL;

	iconv_t cd = invalid, cd2 = invalid;
	iconv_t error_cd = invalid;

	char quest_mark[] = "?";	/* best to leave non-const; implementations vary */
	ICONV_CONST char *pquest_mark = quest_mark;
	size_t lquest_mark;
	size_t irreversible;
	size_t one_character;
	char *p;
	int eilseq_raised = 0;
	/* cast away const-ness */
	TDS_ERRNO_MESSAGE_FLAGS *suppress = (TDS_ERRNO_MESSAGE_FLAGS*) &conv->suppress;

	assert(inbuf && inbytesleft && outbuf && outbytesleft);

	switch (io) {
	case to_server:
		cd = conv->to_wire;
		cd2 = conv->to_wire2;
		input_charset = &conv->client_charset;
		output_charset_name = conv->server_charset.name;
		break;
	case to_client:
		cd = conv->from_wire;
		cd2 = conv->from_wire2;
		input_charset = &conv->server_charset;
		output_charset_name = conv->client_charset.name;
		break;
	default:
		tdsdump_log(TDS_DBG_FUNC, "tds_iconv: unable to determine if %d means in or out.  \n", io);
		assert(io == to_server || io == to_client);
		break;
	}

	/* silly case, memcpy */
	if (conv->flags & TDS_ENCODING_MEMCPY || cd == invalid) {
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
		if (conv->flags & TDS_ENCODING_INDIRECT) {
#if ENABLE_EXTRA_CHECKS
			char tmp[8];
#else
			char tmp[128];
#endif
			char *pb = tmp;
			size_t l = sizeof(tmp);
			int temp_errno;
			size_t temp_irreversible;

			temp_irreversible = tds_sys_iconv(cd, (ICONV_CONST char **) inbuf, inbytesleft, &pb, &l);
			temp_errno = errno;

			/* convert partial */
			pb = tmp;
			l = sizeof(tmp) - l;
			for (;;) {
				errno = 0;
				irreversible = tds_sys_iconv(cd2, (ICONV_CONST char **) &pb, &l, outbuf, outbytesleft);
				if (irreversible != (size_t) - 1) {
					if (inbytesleft && *inbytesleft)
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
				*pb = (char) 0x80;
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
		} else if (io == to_client && conv->flags & TDS_ENCODING_SWAPBYTE && inbuf) {
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
			irreversible = tds_sys_iconv(cd, (ICONV_CONST char **) &pib, &il, outbuf, outbytesleft);
			il = pib - tmp;
			*inbuf += il;
			*inbytesleft -= il;
			if (irreversible != (size_t) - 1 && *inbytesleft)
				continue;
		} else {
			irreversible = tds_sys_iconv(cd, (ICONV_CONST char **) inbuf, inbytesleft, outbuf, outbytesleft);
		}
		/* iconv success, return */
		if (irreversible != (size_t) - 1) {
			/* here we detect end of conversion and try to reset shift state */
			if (inbuf) {
				/*
				 * if inbuf or *inbuf is NULL iconv reset the shift state.
				 * Note that setting inbytesleft to NULL can cause core so don't do it!
				 */
				inbuf = NULL;
				continue;
			}
			break;
		}

		if (errno == EILSEQ)
			eilseq_raised = 1;

		if (errno != EILSEQ || io != to_client || !inbuf)
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
			error_cd = tds_sys_iconv_open(output_charset_name, iconv_names[POS_UTF8]);
			if (error_cd == invalid) {
				break;	/* what to do? */
			}
		}

		lquest_mark = 1;
		pquest_mark = quest_mark;

		p = *outbuf;
		irreversible = tds_sys_iconv(error_cd, &pquest_mark, &lquest_mark, outbuf, outbytesleft);

		if (irreversible == (size_t) - 1)
			break;

		if (!*inbytesleft)
			break;
	}
end_loop:
	
	/* swap bytes if necessary */
	if (io == to_server && conv->flags & TDS_ENCODING_SWAPBYTE) {
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
				tdserror(tds->tds_ctx, tds, TDSEICONV2BIG, 0);
			} else {
				tdserror(tds->tds_ctx, tds, TDSEICONVI, 0);
				errno = 0;
			}
		} else {
			tdserror(tds->tds_ctx, tds, TDSEICONVO, 0);
		}
		suppress->eilseq = 1;
	}

	switch (errno) {
	case EINVAL:		/* incomplete multibyte sequence is encountered */
		if (suppress->einval)
			break;
		/* in chunk conversion this can mean we end a chunk inside a character */
		tdserror(tds->tds_ctx, tds, TDSEICONVAVAIL, 0);
		suppress->einval = 1;
		break;
	case E2BIG:		/* output buffer has no more room */
		if (suppress->e2big)
			break;
		tdserror(tds->tds_ctx, tds, TDSEICONVIU, 0);
		suppress->e2big = 1;
		break;
	default:
		break;
	}

	if (error_cd != invalid) {
		tds_sys_iconv_close(error_cd);
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
#ifdef ENABLE_EXTRA_CHECKS
	char buffer[16];
#else
	char buffer[16000];
#endif
	char *ib;
	size_t isize = 0, nonreversible_conversions = 0;

	/*
	 * If cd isn't valid, it's just an indication that this column needs no conversion.  
	 */
	if (cd == (iconv_t) -1) {
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

	for (ib = buffer; isize && (isize = fread(ib, 1, isize, stream)) > 0;) {

		tdsdump_log(TDS_DBG_FUNC, "tds_iconv_fread: read %u of %u bytes; outbuf has %u left.\n", (unsigned int) isize,
			    (unsigned int) field_len, (unsigned int) *outbytesleft);
		field_len -= isize;

		isize += ib - buffer;
		ib = buffer;
		nonreversible_conversions += tds_sys_iconv(cd, (ICONV_CONST char **) &ib, &isize, &outbuf, outbytesleft);

		if (isize != 0) {
			memmove(buffer, ib, isize);
			switch (errno) {
			case EINVAL:	/* incomplete multibyte sequence encountered in input */
				break;
			case E2BIG:	/* insufficient room in output buffer */
			case EILSEQ:	/* invalid multibyte sequence encountered in input */
			default:
				/* FIXME: emit message */
				tdsdump_log(TDS_DBG_FUNC, "tds_iconv_fread: error %d: %s.\n", errno, strerror(errno));
				break;
			}
		}
		ib = buffer + isize;
		isize = sizeof(buffer) - isize;
		if (isize > field_len)
			isize = field_len;
	}
	
	READ_TERMINATOR:

	if (term_len > 0 && !feof(stream)) {
		isize += term_len;
		if (term_len && 1 == fread(buffer, term_len, 1, stream)) {
			isize -= term_len;
		} else {
			tdsdump_log(TDS_DBG_FUNC, "tds_iconv_fread: cannot read %u-byte terminator\n", (unsigned int) term_len);
		}
	}

	return field_len + isize;
}

/**
 * Get a iconv info structure, allocate and initialize if needed
 */
static TDSICONV *
tds_iconv_get_info(TDSSOCKET * tds, const char *canonic_client_charset, const char *canonic_server_charset)
{
	TDSICONV *info;
	int i;

	/* search a charset from already allocated charsets */
	for (i = tds->char_conv_count; --i >= initial_char_conv_count;)
		if (strcmp(canonic_client_charset, tds->char_convs[i]->client_charset.name) == 0
		    && strcmp(canonic_server_charset, tds->char_convs[i]->server_charset.name) == 0)
			return tds->char_convs[i];

	/* allocate a new iconv structure */
	if (tds->char_conv_count % CHUNK_ALLOC == ((initial_char_conv_count + 1) % CHUNK_ALLOC)) {
		TDSICONV **p;
		TDSICONV *infos;

		infos = (TDSICONV *) malloc(sizeof(TDSICONV) * CHUNK_ALLOC);
		if (!infos)
			return NULL;
		p = (TDSICONV **) realloc(tds->char_convs, sizeof(TDSICONV *) * (tds->char_conv_count + CHUNK_ALLOC));
		if (!p) {
			free(infos);
			return NULL;
		}
		tds->char_convs = p;
		memset(infos, 0, sizeof(TDSICONV) * CHUNK_ALLOC);
		for (i = 0; i < CHUNK_ALLOC; ++i) {
			tds->char_convs[i + tds->char_conv_count] = &infos[i];
			tds_iconv_reset(&infos[i]);
		}
	}
	info = tds->char_convs[tds->char_conv_count++];

	/* init */
	/* TODO test allocation */
	tds_iconv_info_init(info, canonic_client_charset, canonic_server_charset);
	return info;
}

TDSICONV *
tds_iconv_get(TDSSOCKET * tds, const char *client_charset, const char *server_charset)
{
	int canonic_client_charset_num = tds_canonical_charset(client_charset);
	int canonic_server_charset_num = tds_canonical_charset(server_charset);

	if (canonic_client_charset_num < 0) {
		tdsdump_log(TDS_DBG_FUNC, "tds_iconv_get: what is charset \"%s\"?\n", client_charset);
		return NULL;
	}
	if (canonic_server_charset_num < 0) {
		tdsdump_log(TDS_DBG_FUNC, "tds_iconv_get: what is charset \"%s\"?\n", server_charset);
		return NULL;
	}

	return tds_iconv_get_info(tds, canonic_charsets[canonic_client_charset_num].name, canonic_charsets[canonic_server_charset_num].name);
}

/* change singlebyte conversions according to server */
void
tds_srv_charset_changed(TDSSOCKET * tds, const char *charset)
{
#if HAVE_ICONV_ALWAYS
	TDSICONV *char_conv = tds->char_convs[client2server_chardata];

	int canonic_charset_num = tds_canonical_charset(charset);
	const char *canonic_charset;

	if (IS_TDS7_PLUS(tds) && canonic_charset_num == TDS_CHARSET_ISO_8859_1)
		canonic_charset_num = TDS_CHARSET_CP1252;

	/* ignore request to change to unknown charset */
	if (canonic_charset_num < 0) {
		tdsdump_log(TDS_DBG_FUNC, "tds_srv_charset_changed: what is charset \"%s\"?\n", charset);
		return;
	}
	canonic_charset = canonic_charsets[canonic_charset_num].name;

	tdsdump_log(TDS_DBG_FUNC, "setting server single-byte charset to \"%s\"\n", canonic_charset);

	if (strcmp(canonic_charset, char_conv->server_charset.name) == 0)
		return;

	/* find and set conversion */
	char_conv = tds_iconv_get_info(tds, tds->char_convs[client2ucs2]->client_charset.name, canonic_charset);
	if (char_conv)
		tds->char_convs[client2server_chardata] = char_conv;

	/* if sybase change also server conversions */
	if (IS_TDS7_PLUS(tds))
		return;

	char_conv = tds->char_convs[iso2server_metadata];

	tds_iconv_info_close(char_conv);

	tds_iconv_info_init(char_conv, "ISO-8859-1", charset);
#endif
}

/* change singlebyte conversions according to server */
void
tds7_srv_charset_changed(TDSSOCKET * tds, int sql_collate, int lcid)
{
	tds_srv_charset_changed(tds, collate2charset(sql_collate, lcid));
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
static size_t
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
		if (charsize > *input_size)
			return 0;
		*input += charsize;
		*input_size -= charsize;
		return charsize;
	}

	if (0 == strcmp(charset->name, "UTF-8")) {
		/*
		 * Deal with UTF-8.  
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
		if (charsize > *input_size)
			return 0;
		*input += charsize;
		*input_size -= charsize;
		return charsize;
	}

	/* handle state encoding */

	/* extract state from iconv */
	pob = ib;
	ol = sizeof(ib);
	tds_sys_iconv(cd, NULL, NULL, &pob, &ol);

	/* init destination conversion */
	/* TODO use largest fixed size for this platform */
	cd2 = tds_sys_iconv_open("UCS-4", charset->name);
	if (cd2 == (iconv_t) -1)
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
	tds_sys_iconv(cd2, &pib, &il, &pob, &ol);

	/* adjust input */
	l = (pib - ib) - l;
	*input += l;
	*input_size -= l;

	/* extract state */
	pob = ib;
	ol = sizeof(ib);
	tds_sys_iconv(cd, NULL, NULL, &pob, &ol);

	/* set input state */
	pib = ib;
	il = sizeof(ib) - ol;
	pob = ob;
	ol = sizeof(ob);
	tds_sys_iconv(cd, &pib, &il, &pob, &ol);

	tds_sys_iconv_close(cd2);

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
collate2charset(int sql_collate, int lcid)
{
	/*
	 * The table from the MSQLServer reference "Windows Collation Designators" 
	 * and from " NLS Information for Microsoft Windows XP"
	 */

	const char *cp = NULL;

	switch (sql_collate) {
	case 30:		/* SQL_Latin1_General_CP437_BIN */
	case 31:		/* SQL_Latin1_General_CP437_CS_AS */
	case 32:		/* SQL_Latin1_General_CP437_CI_AS */
	case 33:		/* SQL_Latin1_General_Pref_CP437_CI_AS */
	case 34:		/* SQL_Latin1_General_CP437_CI_AI */
		return "CP437";
	case 40:		/* SQL_Latin1_General_CP850_BIN */
	case 41:		/* SQL_Latin1_General_CP850_CS_AS */
	case 42:		/* SQL_Latin1_General_CP850_CI_AS */
	case 43:		/* SQL_Latin1_General_Pref_CP850_CI_AS */
	case 44:		/* SQL_Latin1_General_CP850_CI_AI */
	case 49:		/* SQL_1xCompat_CP850_CI_AS */
	case 55:		/* SQL_AltDiction_CP850_CS_AS */
	case 56:		/* SQL_AltDiction_Pref_CP850_CI_AS */
	case 57:		/* SQL_AltDiction_CP850_CI_AI */
	case 58:		/* SQL_Scandinavian_Pref_CP850_CI_AS */
	case 59:		/* SQL_Scandinavian_CP850_CS_AS */
	case 60:		/* SQL_Scandinavian_CP850_CI_AS */
	case 61:		/* SQL_AltDiction_CP850_CI_AS */
		return "CP850";
	case 81:		/* SQL_Latin1_General_CP1250_CS_AS */
	case 82:		/* SQL_Latin1_General_CP1250_CI_AS */
		return "CP1250";
	case 105:		/* SQL_Latin1_General_CP1251_CS_AS */
	case 106:		/* SQL_Latin1_General_CP1251_CI_AS */
		return "CP1251";
	case 113:		/* SQL_Latin1_General_CP1253_CS_AS */
	case 114:		/* SQL_Latin1_General_CP1253_CI_AS */
	case 120:		/* SQL_MixDiction_CP1253_CS_AS */
	case 121:		/* SQL_AltDiction_CP1253_CS_AS */
	case 122:		/* SQL_AltDiction2_CP1253_CS_AS */
	case 124:		/* SQL_Latin1_General_CP1253_CI_AI */
		return "CP1253";
	case 137:		/* SQL_Latin1_General_CP1255_CS_AS */
	case 138:		/* SQL_Latin1_General_CP1255_CI_AS */
		return "CP1255";
	case 145:		/* SQL_Latin1_General_CP1256_CS_AS */
	case 146:		/* SQL_Latin1_General_CP1256_CI_AS */
		return "CP1256";
	case 153:		/* SQL_Latin1_General_CP1257_CS_AS */
	case 154:		/* SQL_Latin1_General_CP1257_CI_AS */
		return "CP1257";
	}

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
TDSICONV *
tds_iconv_from_collate(TDSSOCKET * tds, TDS_UCHAR collate[5])
{
	const int sql_collate = collate[4];
	const int lcid = collate[1] * 256 + collate[0];
	const char *charset = collate2charset(sql_collate, lcid);

#if ENABLE_EXTRA_CHECKS
	assert(strcmp(tds_canonical_charset_name(charset), charset) == 0);
#endif

	/* same as client (usually this is true, so this improve performance) ? */
	if (strcmp(tds->char_convs[client2server_chardata]->server_charset.name, charset) == 0)
		return tds->char_convs[client2server_chardata];

	return tds_iconv_get_info(tds, tds->char_convs[client2ucs2]->client_charset.name, charset);
}

/** @} */
