/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2006, 2007  Frediano Ziglio
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

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <ctype.h>

#include "tds.h"
#include "tdsiconv.h"
#include "tdsconvert.h"
#include "tds_checks.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include <assert.h>

TDS_RCSID(var, "$Id: query.c,v 1.217.2.4 2008-11-05 14:56:12 freddy77 Exp $");

static void tds_put_params(TDSSOCKET * tds, TDSPARAMINFO * info, int flags);
static void tds7_put_query_params(TDSSOCKET * tds, const char *query, int query_len);
static void tds7_put_params_definition(TDSSOCKET * tds, const char *param_definition, size_t param_length);
static int tds_put_data_info(TDSSOCKET * tds, TDSCOLUMN * curcol, int flags);
static int tds_put_data(TDSSOCKET * tds, TDSCOLUMN * curcol);
static char *tds7_build_param_def_from_query(TDSSOCKET * tds, const char* converted_query, int converted_query_len, TDSPARAMINFO * params, size_t *out_len);
static char *tds7_build_param_def_from_params(TDSSOCKET * tds, const char* query, size_t query_len, TDSPARAMINFO * params, size_t *out_len);

static int tds_send_emulated_execute(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params);
static const char *tds_skip_comment(const char *s);
static int tds_count_placeholders_ucs2le(const char *query, const char *query_end);

#define TDS_PUT_DATA_USE_NAME 1
#define TDS_PUT_DATA_PREFIX_NAME 2

#undef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#undef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

/* All manner of client to server submittal functions */

/**
 * \ingroup libtds
 * \defgroup query Query
 * Function to handle query.
 */

/**
 * \addtogroup query
 * @{ 
 */

/**
 * Accept an ASCII string, convert it to UCS2-LE
 * The input is null-terminated, but the output excludes the null.
 * \param buffer buffer where to store output
 * \param buf string to write
 * \return bytes written
 */
static int
tds_ascii_to_ucs2(char *buffer, const char *buf)
{
	char *s;
	assert(buffer && buf && *buf); /* This is an internal function.  Call it correctly. */

	for (s = buffer; *buf != '\0'; ++buf) {
		*s++ = *buf;
		*s++ = '\0';
	}

	return s - buffer;
}

#define TDS_PUT_N_AS_UCS2(tds, s) do { \
	char buffer[sizeof(s)*2-2]; \
	tds_put_n(tds, buffer, tds_ascii_to_ucs2(buffer, s)); \
} while(0)

/**
 * Convert a string in an allocated buffer
 * \param tds        state information for the socket and the TDS protocol
 * \param char_conv  information about the encodings involved
 * \param s          input string
 * \param len        input string length (in bytes), -1 for null terminated
 * \param out_len    returned output length (in bytes)
 * \return string allocated (or input pointer if no conversion required) or NULL if error
 */
static const char *
tds_convert_string(TDSSOCKET * tds, const TDSICONV * char_conv, const char *s, int len, int *out_len)
{
	char *buf;

	const char *ib;
	char *ob;
	size_t il, ol;

	/* char_conv is only mostly const */
	TDS_ERRNO_MESSAGE_FLAGS *suppress = (TDS_ERRNO_MESSAGE_FLAGS*) &char_conv->suppress;

	CHECK_TDS_EXTRA(tds);

	if (len < 0)
		len = strlen(s);
	if (char_conv->flags == TDS_ENCODING_MEMCPY) {
		*out_len = len;
		return s;
	}

	/* allocate needed buffer (+1 is to exclude 0 case) */
	ol = len * char_conv->server_charset.max_bytes_per_char / char_conv->client_charset.min_bytes_per_char + 1;
	buf = (char *) malloc(ol);
	if (!buf)
		return NULL;

	ib = s;
	il = len;
	ob = buf;
	memset(suppress, 0, sizeof(char_conv->suppress));
	if (tds_iconv(tds, char_conv, to_server, &ib, &il, &ob, &ol) == (size_t)-1) {
		free(buf);
		return NULL;
	}
	*out_len = ob - buf;
	return buf;
}

#if ENABLE_EXTRA_CHECKS
static void
tds_convert_string_free(const char *original, const char *converted)
{
	if (original != converted)
		free((char *) converted);
}
#else
#define tds_convert_string_free(original, converted) \
	do { if (original != converted) free((char*) converted); } while(0)
#endif

static int
tds_query_flush_packet(TDSSOCKET *tds)
{
	/* TODO depend on result ?? */
	tds_set_state(tds, TDS_PENDING);
	return tds_flush_packet(tds);
}

/**
 * tds_submit_query() sends a language string to the database server for
 * processing.  TDS 4.2 is a plain text message with a packet type of 0x01,
 * TDS 7.0 is a unicode string with packet type 0x01, and TDS 5.0 uses a 
 * TDS_LANGUAGE_TOKEN to encapsulate the query and a packet type of 0x0f.
 * \param tds state information for the socket and the TDS protocol
 * \param query language query to submit
 * \return TDS_FAIL or TDS_SUCCEED
 */
int
tds_submit_query(TDSSOCKET * tds, const char *query)
{
	return tds_submit_query_params(tds, query, NULL);
}

static char *
tds5_fix_dot_query(const char *query, int *query_len, TDSPARAMINFO * params)
{
	int i, pos, l;
	const char *e, *s;
	int size = *query_len + 30;
	char *out = (char *) malloc(size);
	if (!out)
		return NULL;
	pos = 0;

	s = query;
	for (i = 0;; ++i) {
		e = tds_next_placeholder(s);
		l = e ? e - s : strlen(s);
		if (pos + l + 12 >= size) {
			char *p;
			size = pos + l + 30;
			p = realloc(out, size);
			if (!p) {
				free(out);
				return NULL;
			}
			out = p;
		}
		memcpy(out + pos, s, l);
		pos += l;
		if (!e)
			break;
		pos += sprintf(out + pos, "@P%d", i + 1);
		if (i >= params->num_cols) {
			free(out);
			return NULL;
		}
		sprintf(params->columns[i]->column_name, "@P%d", i + 1);
		params->columns[i]->column_namelen = strlen(params->columns[i]->column_name);

		s = e + 1;
	}
	out[pos] = 0;
	*query_len = pos;
	return out;
}

#ifdef ENABLE_DEVELOPING
static const TDS_UCHAR tds9_query_start[] = {
	/* total length */
	0x16, 0, 0, 0,
	/* length */
	0x12, 0, 0, 0,
	/* type */
	0x02, 0,
	/* transaction */
	0, 0, 0, 0, 0, 0, 0, 0,
	/* request count */
	1, 0, 0, 0
};

#define START_QUERY \
do { \
	if (IS_TDS90(tds)) \
		tds_start_query(tds); \
} while(0)

static void
tds_start_query(TDSSOCKET *tds)
{
	tds_put_n(tds, tds9_query_start, 10);
	tds_put_n(tds, tds->tds9_transaction, 8);
	tds_put_n(tds, tds9_query_start + 10 + 8, 4);
}
#else
#define START_QUERY do { ; } while(0)
#endif

/**
 * tds_submit_query_params() sends a language string to the database server for
 * processing.  TDS 4.2 is a plain text message with a packet type of 0x01,
 * TDS 7.0 is a unicode string with packet type 0x01, and TDS 5.0 uses a
 * TDS_LANGUAGE_TOKEN to encapsulate the query and a packet type of 0x0f.
 * \param tds state information for the socket and the TDS protocol
 * \param query  language query to submit
 * \param params parameters of query
 * \return TDS_FAIL or TDS_SUCCEED
 */
int
tds_submit_query_params(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params)
{
	int query_len;
	int num_params = params ? params->num_cols : 0;
 
	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);
 
	if (!query)
		return TDS_FAIL;
 
	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		return TDS_FAIL;
 
	query_len = strlen(query);
 
	if (IS_TDS50(tds)) {
		char *new_query = NULL;
		/* are there '?' style parameters ? */
		if (tds_next_placeholder(query)) {
			if ((new_query = tds5_fix_dot_query(query, &query_len, params)) == NULL) {
				tds_set_state(tds, TDS_IDLE);
				return TDS_FAIL;
			}
			query = new_query;
		}

		tds->out_flag = TDS_NORMAL;
		tds_put_byte(tds, TDS_LANGUAGE_TOKEN);
		/* TODO ICONV use converted size, not input size and convert string */
		tds_put_int(tds, query_len + 1);
		tds_put_byte(tds, params ? 1 : 0);  /* 1 if there are params, 0 otherwise */
		tds_put_n(tds, query, query_len);
		if (params) {
			/* add on parameters */
			tds_put_params(tds, params, params->columns[0]->column_name[0] ? TDS_PUT_DATA_USE_NAME : 0);
		}
		free(new_query);
	} else if (!IS_TDS7_PLUS(tds) || !params || !params->num_cols) {
		tds->out_flag = TDS_QUERY;
		START_QUERY;
		tds_put_string(tds, query, query_len);
	} else {
		TDSCOLUMN *param;
		size_t definition_len;
		int count, i;
		char *param_definition;
		int converted_query_len;
		const char *converted_query;
 
		converted_query = tds_convert_string(tds, tds->char_convs[client2ucs2], query, query_len, &converted_query_len);
		if (!converted_query) {
			tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}

		count = tds_count_placeholders_ucs2le(converted_query, converted_query + converted_query_len);
 
		if (!count) {
			param_definition = tds7_build_param_def_from_params(tds, converted_query, converted_query_len, params, &definition_len);
			if (!param_definition) {
				tds_convert_string_free(query, converted_query);
				tds_set_state(tds, TDS_IDLE);
				return TDS_FAIL;
			}
		} else {
			/*
			 * TODO perhaps functions that calls tds7_build_param_def_from_query
			 * should call also tds7_build_param_def_from_params ??
			 */
			param_definition = tds7_build_param_def_from_query(tds, converted_query, converted_query_len, params, &definition_len);
			if (!param_definition) {
				tds_set_state(tds, TDS_IDLE);
				return TDS_FAIL;
			}
		}
 
		tds->out_flag = TDS_RPC;
		START_QUERY;
		/* procedure name */
		if (IS_TDS8_PLUS(tds)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_EXECUTESQL);
		} else {
			tds_put_smallint(tds, 13);
			TDS_PUT_N_AS_UCS2(tds, "sp_executesql");
		}
		tds_put_smallint(tds, 0);
 
		/* string with sql statement */
		if (!count) {
			tds_put_byte(tds, 0);
			tds_put_byte(tds, 0);
			tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
			tds_put_int(tds, converted_query_len);
			if (IS_TDS8_PLUS(tds))
				tds_put_n(tds, tds->collation, 5);
			tds_put_int(tds, converted_query_len);
			tds_put_n(tds, converted_query, converted_query_len);
		} else {
			tds7_put_query_params(tds, converted_query, converted_query_len);
		}
		tds_convert_string_free(query, converted_query);
 
		tds7_put_params_definition(tds, param_definition, definition_len);
		free(param_definition);
 
		for (i = 0; i < num_params; i++) {
			param = params->columns[i];
			/* TODO check error */
			tds_put_data_info(tds, param, 0);
			/* FIXME handle error */
			tds_put_data(tds, param);
		}
		tds->internal_sp_called = TDS_SP_EXECUTESQL;
	}
	return tds_query_flush_packet(tds);
}


int
tds_submit_queryf(TDSSOCKET * tds, const char *queryf, ...)
{
	va_list ap;
	char *query = NULL;
	int rc = TDS_FAIL;

	CHECK_TDS_EXTRA(tds);

	va_start(ap, queryf);
	if (vasprintf(&query, queryf, ap) >= 0) {
		rc = tds_submit_query(tds, query);
		free(query);
	}
	va_end(ap);
	return rc;
}

static const char *
tds_skip_comment(const char *s)
{
	const char *p = s;

	if (*p == '-' && p[1] == '-') {
		for (;*++p != '\0';)
			if (*p == '\n')
				return p;
	} else if (*p == '/' && p[1] == '*') {
		++p;
		for(;*++p != '\0';)
			if (*p == '*' && p[1] == '/')
				return p + 2;
	} else
		++p;

	return p;
}

/**
 * Skip quoting string (like 'sfsf', "dflkdj" or [dfkjd])
 * @param s pointer to first quoting character (should be '," or [)
 * @return character after quoting
 */
const char *
tds_skip_quoted(const char *s)
{
	const char *p = s;
	char quote = (*s == '[') ? ']' : *s;

	for (; *++p;) {
		if (*p == quote) {
			if (*++p != quote)
				return p;
		}
	}
	return p;
}

/**
 * Get position of next placeholder
 * @param start pointer to part of query to search
 * @return next placeholder or NULL if not found
 */
const char *
tds_next_placeholder(const char *start)
{
	const char *p = start;

	if (!p)
		return NULL;

	for (;;) {
		switch (*p) {
		case '\0':
			return NULL;
		case '\'':
		case '\"':
		case '[':
			p = tds_skip_quoted(p);
			break;

		case '-':
		case '/':
			p = tds_skip_comment(p);
			break;

		case '?':
			return p;
		default:
			++p;
			break;
		}
	}
}

/**
 * Count the number of placeholders in query
 */
int
tds_count_placeholders(const char *query)
{
	const char *p = query - 1;
	int count = 0;

	for (;; ++count) {
		if (!(p = tds_next_placeholder(p + 1)))
			return count;
	}
}

static const char *
tds_skip_comment_ucs2le(const char *s, const char *end)
{
	const char *p = s;

	if (p+4 <= end && memcmp(p, "-\0-", 4) == 0) {
		for (;(p+=2) < end;)
			if (p[0] == '\n' && p[1] == 0)
				return p + 2;
	} else if (p+4 <= end && memcmp(p, "/\0*", 4) == 0) {
		p += 2;
		end -= 2;
		for(;(p+=2) < end;)
			if (memcmp(p, "*\0/", 4) == 0)
				return p + 4;
	} else
		p += 2;

	return p;
}


static const char *
tds_skip_quoted_ucs2le(const char *s, const char *end)
{
	const char *p = s;
	char quote = (*s == '[') ? ']' : *s;

	assert(s[1] == 0 && s < end && (end - s) % 2 == 0);

	for (; (p += 2) != end;) {
		if (p[0] == quote && !p[1]) {
			p += 2;
			if (p == end || p[0] != quote || p[1])
				return p;
		}
	}
	return p;
}

static const char *
tds_next_placeholder_ucs2le(const char *start, const char *end, int named)
{
	const char *p = start;
	char prev = ' ', c;

	assert(p && start <= end && (end - start) % 2 == 0);

	for (; p != end;) {
		if (p[1]) {
			prev = ' ';
			p += 2;
			continue;
		}
		c = p[0];
		switch (c) {
		case '\'':
		case '\"':
		case '[':
			p = tds_skip_quoted_ucs2le(p, end);
			break;

		case '-':
		case '/':
			p = tds_skip_comment_ucs2le(p, end);
			c = ' ';
			break;

		case '?':
			return p;
		case '@':
			if (named && !isalnum((unsigned char) prev))
				return p;
		default:
			p += 2;
			break;
		}
		prev = c;
	}
	return end;
}

static int
tds_count_placeholders_ucs2le(const char *query, const char *query_end)
{
	const char *p = query - 2;
	int count = 0;

	for (;; ++count) {
		if ((p = tds_next_placeholder_ucs2le(p + 2, query_end, 0)) == query_end)
			return count;
	}
}

/**
 * Return declaration for column (like "varchar(20)")
 * \param tds    state information for the socket and the TDS protocol
 * \param curcol column
 * \param out    buffer to hold declaration
 * \return TDS_FAIL or TDS_SUCCEED
 */
static int
tds_get_column_declaration(TDSSOCKET * tds, TDSCOLUMN * curcol, char *out)
{
	const char *fmt = NULL;
	int max_len = IS_TDS7_PLUS(tds) ? 8000 : 255;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(curcol);

	switch (tds_get_conversion_type(curcol->on_server.column_type, curcol->on_server.column_size)) {
	case XSYBCHAR:
	case SYBCHAR:
		fmt = "CHAR(%d)";
		break;
	case SYBVARCHAR:
	case XSYBVARCHAR:
		fmt = "VARCHAR(%d)";
		break;
	case SYBINT1:
		fmt = "TINYINT";
		break;
	case SYBINT2:
		fmt = "SMALLINT";
		break;
	case SYBINT4:
		fmt = "INT";
		break;
	case SYBINT8:
		/* TODO even for Sybase ?? */
		fmt = "BIGINT";
		break;
	case SYBFLT8:
		fmt = "FLOAT";
		break;
	case SYBDATETIME:
		fmt = "DATETIME";
		break;
	case SYBBIT:
		fmt = "BIT";
		break;
	case SYBTEXT:
		fmt = "TEXT";
		break;
	case SYBLONGBINARY:	/* TODO correct ?? */
	case SYBIMAGE:
		fmt = "IMAGE";
		break;
	case SYBMONEY4:
		fmt = "SMALLMONEY";
		break;
	case SYBMONEY:
		fmt = "MONEY";
		break;
	case SYBDATETIME4:
		fmt = "SMALLDATETIME";
		break;
	case SYBREAL:
		fmt = "REAL";
		break;
	case SYBBINARY:
	case XSYBBINARY:
		fmt = "BINARY(%d)";
		break;
	case SYBVARBINARY:
	case XSYBVARBINARY:
		fmt = "VARBINARY(%d)";
		break;
	case SYBNUMERIC:
		fmt = "NUMERIC(%d,%d)";
		goto numeric_decimal;
	case SYBDECIMAL:
		fmt = "DECIMAL(%d,%d)";
	      numeric_decimal:
		sprintf(out, fmt, curcol->column_prec, curcol->column_scale);
		return TDS_SUCCEED;
		break;
	case SYBUNIQUE:
		if (IS_TDS7_PLUS(tds))
			fmt = "UNIQUEIDENTIFIER";
		break;
	case SYBNTEXT:
		if (IS_TDS7_PLUS(tds))
			fmt = "NTEXT";
		break;
	case SYBNVARCHAR:
	case XSYBNVARCHAR:
		if (IS_TDS7_PLUS(tds)) {
			fmt = "NVARCHAR(%d)";
			max_len = 4000;
		}
		break;
	case XSYBNCHAR:
		if (IS_TDS7_PLUS(tds)) {
			fmt = "NCHAR(%d)";
			max_len = 4000;
		}
		break;
		/* nullable types should not occur here... */
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	case SYBBITN:
	case SYBINTN:
		assert(0);
		/* TODO... */
	case SYBVOID:
	case SYBSINT1:
	case SYBUINT2:
	case SYBUINT4:
	case SYBUINT8:
	case SYBVARIANT:
		break;
	}

	if (fmt) {
		TDS_INT size = curcol->on_server.column_size;
		if (!size)
			size = curcol->column_size;
		/* fill out */
		sprintf(out, fmt, size > 0 ? (size > max_len ? max_len : size) : 1);
		return TDS_SUCCEED;
	}

	out[0] = 0;
	return TDS_FAIL;
}

/**
 * Return string with parameters definition, useful for TDS7+
 * \param tds     state information for the socket and the TDS protocol
 * \param params  parameters to build declaration
 * \param out_len length output buffer in bytes
 * \return allocated and filled string or NULL on failure (coded in ucs2le charset )
 */
/* TODO find a better name for this function */
static char *
tds7_build_param_def_from_query(TDSSOCKET * tds, const char* converted_query, int converted_query_len, TDSPARAMINFO * params, size_t *out_len)
{
	size_t size = 512;
	char *param_str;
	char *p;
	char declaration[40];
	size_t l = 0;
	int i, count;

	assert(IS_TDS7_PLUS(tds));
	assert(out_len);

	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);

	count = tds_count_placeholders_ucs2le(converted_query, converted_query + converted_query_len);
	
	param_str = (char *) malloc(512);
	if (!param_str)
		return NULL;

	for (i = 0; i < count; ++i) {
		if (l > 0u) {
			param_str[l++] = ',';
			param_str[l++] = 0;
		}

		/* realloc on insufficient space */
		while ((l + (2u * 40u)) > size) {
			p = (char *) realloc(param_str, size += 512u);
			if (!p)
				goto Cleanup;
			param_str = p;
		}

		/* get this parameter declaration */
		sprintf(declaration, "@P%d ", i+1);
		if (params && i < params->num_cols) {
			if (tds_get_column_declaration(tds, params->columns[i], declaration + strlen(declaration)) == TDS_FAIL)
				goto Cleanup;
		} else {
			strcat(declaration, "varchar(80)");
		}

		/* convert it to ucs2 and append */
		l += tds_ascii_to_ucs2(param_str + l, declaration);
	}
	*out_len = l;
	return param_str;

      Cleanup:
	free(param_str);
	return NULL;
}

/**
 * Return string with parameters definition, useful for TDS7+
 * \param tds     state information for the socket and the TDS protocol
 * \param params  parameters to build declaration
 * \param out_len length output buffer in bytes
 * \return allocated and filled string or NULL on failure (coded in ucs2le charset )
 */
/* TODO find a better name for this function */
static char *
tds7_build_param_def_from_params(TDSSOCKET * tds, const char* query, size_t query_len,  TDSPARAMINFO * params, size_t *out_len)
{
	size_t size = 512;
	char *param_str;
	char *p;
	char declaration[40];
	size_t l = 0;
	int i;
	struct tds_ids {
		const char *p;
		size_t len;
	} *ids = NULL;
 
	assert(IS_TDS7_PLUS(tds));
	assert(out_len);
 
	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);
 
	param_str = (char *) malloc(512);
	if (!param_str)
		return NULL;

	/* try to detect missing names */
	if (params->num_cols) {
		ids = (struct tds_ids *) calloc(params->num_cols, sizeof(struct tds_ids));
		if (!ids)
			goto Cleanup;
		if (!params->columns[0]->column_name[0]) {
			const char *s = query, *e, *id_end;
			const char *query_end = query + query_len;

			for (i = 0;  i < params->num_cols; s = e + 2) {
				e = tds_next_placeholder_ucs2le(s, query_end, 1);
				if (e == query_end)
					break;
				if (e[0] != '@')
					continue;
				/* find end of param name */
				for (id_end = e + 2; id_end != query_end; id_end += 2)
					if (!id_end[1] && (id_end[0] != '_' && id_end[1] != '#' && !isalnum((unsigned char) id_end[0])))
						break;
				ids[i].p = e;
				ids[i].len = id_end - e;
				++i;
			}
		}
	}
 
	for (i = 0; i < params->num_cols; ++i) {
		const char *ib;
		char *ob;
		size_t il, ol;
 
		if (l > 0u) {
			param_str[l++] = ',';
			param_str[l++] = 0;
		}
 
		/* realloc on insufficient space */
		il = ids[i].p ? ids[i].len : 2 * params->columns[i]->column_namelen;
		while ((l + (2u * 26u) + il) > size) {
			p = (char *) realloc(param_str, size += 512);
			if (!p)
				goto Cleanup;
			param_str = p;
		}
 
		/* this part of buffer can be not-ascii compatible, use all ucs2... */
		if (ids[i].len) {
			memcpy(param_str + l, ids[i].p, ids[i].len);
			l += ids[i].len;
		} else {
			ib = params->columns[i]->column_name;
			il = params->columns[i]->column_namelen;
			ob = param_str + l;
			ol = size - l;
			memset(&tds->char_convs[iso2server_metadata]->suppress, 0, sizeof(tds->char_convs[iso2server_metadata]->suppress));
			if (tds_iconv(tds, tds->char_convs[iso2server_metadata], to_server, &ib, &il, &ob, &ol) == (size_t) - 1)
				goto Cleanup;
			l = size - ol;
		}
		param_str[l++] = ' ';
		param_str[l++] = 0;
 
		/* get this parameter declaration */
		tds_get_column_declaration(tds, params->columns[i], declaration);
		if (!declaration[0])
			goto Cleanup;
 
		/* convert it to ucs2 and append */
		l += tds_ascii_to_ucs2(param_str + l, declaration);
 
	}
	free(ids);
	*out_len = l;
	return param_str;
 
      Cleanup:
	free(ids);
	free(param_str);
	return NULL;
}


/**
 * Output params types and query (required by sp_prepare/sp_executesql/sp_prepexec)
 * \param tds       state information for the socket and the TDS protocol
 * \param query     query (in ucs2le codings)
 * \param query_len query length in bytes
 */
static void
tds7_put_query_params(TDSSOCKET * tds, const char *query, int query_len)
{
	int len, i, num_placeholders;
	const char *s, *e;
	char buf[24];
	const char *const query_end = query + query_len;

	CHECK_TDS_EXTRA(tds);

	assert(IS_TDS7_PLUS(tds));

	/* we use all "@PX" for parameters */
	num_placeholders = tds_count_placeholders_ucs2le(query, query_end);
	len = num_placeholders * 2;
	/* adjust for the length of X */
	for (i = 10; i <= num_placeholders; i *= 10) {
		len += num_placeholders - i + 1;
	}

	/* string with sql statement */
	/* replace placeholders with dummy parametes */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
	len = 2 * len + query_len;
	tds_put_int(tds, len);
	if (IS_TDS8_PLUS(tds))
		tds_put_n(tds, tds->collation, 5);
	tds_put_int(tds, len);
	s = query;
	/* TODO do a test with "...?" and "...?)" */
	for (i = 1;; ++i) {
		e = tds_next_placeholder_ucs2le(s, query_end, 0);
		assert(e && query <= e && e <= query_end);
		tds_put_n(tds, s, e - s);
		if (e == query_end)
			break;
		sprintf(buf, "@P%d", i);
		tds_put_string(tds, buf, -1);
		s = e + 2;
	}
}

static void
tds7_put_params_definition(TDSSOCKET * tds, const char *param_definition, size_t param_length)
{
	CHECK_TDS_EXTRA(tds);

	/* string with parameters types */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */

	/* put parameters definitions */
	tds_put_int(tds, param_length);
	if (IS_TDS8_PLUS(tds))
		tds_put_n(tds, tds->collation, 5);
	tds_put_int(tds, param_length ? param_length : -1);
	tds_put_n(tds, param_definition, param_length);
}

/**
 * tds_submit_prepare() creates a temporary stored procedure in the server.
 * Under TDS 4.2 dynamic statements are emulated building sql command
 * \param tds     state information for the socket and the TDS protocol
 * \param query   language query with given placeholders (?)
 * \param id      string to identify the dynamic query. Pass NULL for automatic generation.
 * \param dyn_out will receive allocated TDSDYNAMIC*. Any older allocated dynamic won't be freed, Can be NULL.
 * \param params  parameters to use. It can be NULL even if parameters are present. Used only for TDS7+
 * \return TDS_FAIL or TDS_SUCCEED
 */
/* TODO parse all results ?? */
int
tds_submit_prepare(TDSSOCKET * tds, const char *query, const char *id, TDSDYNAMIC ** dyn_out, TDSPARAMINFO * params)
{
	int id_len, query_len;
	int rc;
	TDSDYNAMIC *dyn;

	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);

	if (!query)
		return TDS_FAIL;

	/* allocate a structure for this thing */
	dyn = tds_alloc_dynamic(tds, id);
	if (!dyn)
		return TDS_FAIL;
	
	/* TDS5 sometimes cannot accept prepare so we need to store query */
	if (!IS_TDS7_PLUS(tds)) {
		dyn->query = strdup(query);
		if (!dyn->query) {
			tds_free_dynamic(tds, dyn);
			return TDS_FAIL;
		}
	}

	tds->cur_dyn = dyn;

	if (dyn_out)
		*dyn_out = dyn;

	if (!IS_TDS50(tds) && !IS_TDS7_PLUS(tds)) {
		dyn->emulated = 1;
		return TDS_SUCCEED;
	}

	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		goto failure_nostate;

	query_len = strlen(query);

	if (IS_TDS7_PLUS(tds)) {
		size_t definition_len = 0;
		char *param_definition = NULL;
		int converted_query_len;
		const char *converted_query;

		converted_query = tds_convert_string(tds, tds->char_convs[client2ucs2], query, query_len, &converted_query_len);
		if (!converted_query)
			goto failure;

		param_definition = tds7_build_param_def_from_query(tds, converted_query, converted_query_len, params, &definition_len);
		if (!param_definition) {
			tds_convert_string_free(query, converted_query);
			goto failure;
		}

		tds->out_flag = TDS_RPC;
		START_QUERY;
		/* procedure name */
		if (IS_TDS8_PLUS(tds)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_PREPARE);
		} else {
			tds_put_smallint(tds, 10);
			TDS_PUT_N_AS_UCS2(tds, "sp_prepare");
		}
		tds_put_smallint(tds, 0);

		/* return param handle (int) */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 1);	/* result */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 0);

		tds7_put_params_definition(tds, param_definition, definition_len);
		tds7_put_query_params(tds, converted_query, converted_query_len);
		tds_convert_string_free(query, converted_query);
		free(param_definition);

		/* 1 param ?? why ? flags ?? */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 0);
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, 1);

		tds->internal_sp_called = TDS_SP_PREPARE;
	} else {

		tds->out_flag = TDS_NORMAL;

		id_len = strlen(dyn->id);
		tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
		tds_put_smallint(tds, query_len + id_len * 2 + 21);
		tds_put_byte(tds, 0x01);
		tds_put_byte(tds, 0x00);
		tds_put_byte(tds, id_len);
		tds_put_n(tds, dyn->id, id_len);
		/* TODO ICONV convert string, do not put with tds_put_n */
		/* TODO how to pass parameters type? like store procedures ? */
		tds_put_smallint(tds, query_len + id_len + 16);
		tds_put_n(tds, "create proc ", 12);
		tds_put_n(tds, dyn->id, id_len);
		tds_put_n(tds, " as ", 4);
		tds_put_n(tds, query, query_len);
	}

	rc = tds_query_flush_packet(tds);
	if (rc != TDS_FAIL)
		return rc;

failure:
	/* TODO correct if writing fail ?? */
	tds_set_state(tds, TDS_IDLE);

failure_nostate:
	tds->cur_dyn = NULL;
	tds_free_dynamic(tds, dyn);
	if (dyn_out)
		*dyn_out = NULL;
	return TDS_FAIL;
}

/**
 * Submit a prepared query with parameters
 * \param tds     state information for the socket and the TDS protocol
 * \param query   language query with given placeholders (?)
 * \param params  parameters to send
 * \return TDS_FAIL or TDS_SUCCEED
 */
int
tds_submit_execdirect(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params)
{
	int query_len;
	TDSCOLUMN *param;
	TDSDYNAMIC *dyn;
	int id_len;

	CHECK_TDS_EXTRA(tds);
	CHECK_PARAMINFO_EXTRA(params);

	if (!query)
		return TDS_FAIL;
	query_len = strlen(query);

	if (IS_TDS7_PLUS(tds)) {
		size_t definition_len = 0;
		int i;
		char *param_definition = NULL;
		int converted_query_len;
		const char *converted_query;

		if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
			return TDS_FAIL;

		converted_query = tds_convert_string(tds, tds->char_convs[client2ucs2], query, query_len, &converted_query_len);
		if (!converted_query) {
			tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}

		param_definition = tds7_build_param_def_from_query(tds, converted_query, converted_query_len, params, &definition_len);
		if (!param_definition) {
			tds_convert_string_free(query, converted_query);
			tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}

		tds->out_flag = TDS_RPC;
		START_QUERY;
		/* procedure name */
		if (IS_TDS8_PLUS(tds)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_EXECUTESQL);
		} else {
			tds_put_smallint(tds, 13);
			TDS_PUT_N_AS_UCS2(tds, "sp_executesql");
		}
		tds_put_smallint(tds, 0);

		tds7_put_query_params(tds, converted_query, converted_query_len);
		tds7_put_params_definition(tds, param_definition, definition_len);
		tds_convert_string_free(query, converted_query);
		free(param_definition);

		for (i = 0; i < params->num_cols; i++) {
			param = params->columns[i];
			/* TODO check error */
			tds_put_data_info(tds, param, 0);
			/* FIXME handle error */
			tds_put_data(tds, param);
		}

		tds->internal_sp_called = TDS_SP_EXECUTESQL;
		return tds_query_flush_packet(tds);
	}

	/* allocate a structure for this thing */
	dyn = tds_alloc_dynamic(tds, NULL);

	if (!dyn)
		return TDS_FAIL;
	/* check is no parameters */
	if (params &&  !params->num_cols)
		params = NULL;

	/* TDS 4.2, emulate prepared statements */
	/*
	 * TODO Sybase seems to not support parameters in prepared execdirect
	 * so use language or prepare and then exec
	 */
	if (!IS_TDS50(tds) || params) {
		int ret = TDS_SUCCEED;

		dyn->emulated = 1;
		dyn->params = params;
		dyn->query = strdup(query);
		if (!dyn->query)
			ret = TDS_FAIL;
		if (ret != TDS_FAIL)
			if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
				ret = TDS_FAIL;
		if (ret != TDS_FAIL) {
			ret = tds_send_emulated_execute(tds, dyn->query, dyn->params);
			if (ret == TDS_SUCCEED)
				ret = tds_query_flush_packet(tds);
		}
		/* do not free our parameters */
		dyn->params = NULL;
		tds_free_dynamic(tds, dyn);
		return ret;
	}

	tds->cur_dyn = dyn;

	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		return TDS_FAIL;

	tds->out_flag = TDS_NORMAL;

	id_len = strlen(dyn->id);
	tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
	tds_put_smallint(tds, query_len + id_len * 2 + 21);
	tds_put_byte(tds, 0x08);
	tds_put_byte(tds, params ? 0x01 : 0);
	tds_put_byte(tds, id_len);
	tds_put_n(tds, dyn->id, id_len);
	/* TODO ICONV convert string, do not put with tds_put_n */
	/* TODO how to pass parameters type? like store procedures ? */
	tds_put_smallint(tds, query_len + id_len + 16);
	tds_put_n(tds, "create proc ", 12);
	tds_put_n(tds, dyn->id, id_len);
	tds_put_n(tds, " as ", 4);
	tds_put_n(tds, query, query_len);

	if (params)
		tds_put_params(tds, params, 0);

	return tds_flush_packet(tds);
}


/**
 * Put data information to wire
 * \param tds    state information for the socket and the TDS protocol
 * \param curcol column where to store information
 * \param flags  bit flags on how to send data (use TDS_PUT_DATA_USE_NAME for use name information)
 * \return TDS_SUCCEED or TDS_FAIL
 */
static int
tds_put_data_info(TDSSOCKET * tds, TDSCOLUMN * curcol, int flags)
{
	int len;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(curcol);

	if (flags & TDS_PUT_DATA_USE_NAME) {
		len = curcol->column_namelen;
		tdsdump_log(TDS_DBG_ERROR, "tds_put_data_info putting param_name \n");

		if (IS_TDS7_PLUS(tds)) {
			int converted_param_len;
			const char *converted_param;

			/* TODO use a fixed buffer to avoid error ? */
			converted_param =
				tds_convert_string(tds, tds->char_convs[client2ucs2], curcol->column_name, len,
						   &converted_param_len);
			if (!converted_param)
				return TDS_FAIL;
			if (!(flags & TDS_PUT_DATA_PREFIX_NAME)) {
				tds_put_byte(tds, converted_param_len / 2);
			} else {
				tds_put_byte(tds, converted_param_len / 2 + 1);
				tds_put_n(tds, "@", 2);
			}
			tds_put_n(tds, converted_param, converted_param_len);
			tds_convert_string_free(curcol->column_name, converted_param);
		} else {
			/* TODO ICONV convert */
			tds_put_byte(tds, len);	/* param name len */
			tds_put_n(tds, curcol->column_name, len);
		}
	} else {
		tds_put_byte(tds, 0x00);	/* param name len */
	}
	/*
	 * TODO support other flags (use defaul null/no metadata)
	 * bit 1 (2 as flag) in TDS7+ is "default value" bit 
	 * (what's the meaning of "default value" ?)
	 */

	tdsdump_log(TDS_DBG_ERROR, "tds_put_data_info putting status \n");
	tds_put_byte(tds, curcol->column_output);	/* status (input) */
	if (!IS_TDS7_PLUS(tds))
		tds_put_int(tds, curcol->column_usertype);	/* usertype */
	tds_put_byte(tds, curcol->on_server.column_type);

	if (is_numeric_type(curcol->on_server.column_type)) {
#if 1
		tds_put_byte(tds, tds_numeric_bytes_per_prec[curcol->column_prec]);
		tds_put_byte(tds, curcol->column_prec);
		tds_put_byte(tds, curcol->column_scale);
#else
		TDS_NUMERIC *num = (TDS_NUMERIC *) curcol->column_data;
		tds_put_byte(tds, tds_numeric_bytes_per_prec[num->precision]);
		tds_put_byte(tds, num->precision);
		tds_put_byte(tds, num->scale);
#endif
	} else {
		switch (curcol->column_varint_size) {
		case 0:
			break;
		case 1:
			tds_put_byte(tds, MAX(MIN(curcol->column_size, 255), 1));
			break;
		case 2:
			tds_put_smallint(tds, MAX(MIN(curcol->column_size, 8000), 1));
			break;
		case 4:
			tds_put_int(tds, MAX(MIN(curcol->column_size, 0x7fffffff), 1));
			break;
		}
	}

	/* TDS8 output collate information */
	if (IS_TDS8_PLUS(tds) && is_collate_type(curcol->on_server.column_type))
		tds_put_n(tds, tds->collation, 5);

	/* TODO needed in TDS4.2 ?? now is called only is TDS >= 5 */
	if (!IS_TDS7_PLUS(tds)) {

		tdsdump_log(TDS_DBG_ERROR, "HERE! \n");
		tds_put_byte(tds, 0x00);	/* locale info length */
	}
	return TDS_SUCCEED;
}

/**
 * Calc information length in bytes (useful for calculating full packet length)
 * \param tds    state information for the socket and the TDS protocol
 * \param curcol column where to store information
 * \param flags  bit flags on how to send data (use TDS_PUT_DATA_USE_NAME for use name information)
 * \return TDS_SUCCEED or TDS_FAIL
 */
static int
tds_put_data_info_length(TDSSOCKET * tds, TDSCOLUMN * curcol, int flags)
{
	int len = 8;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(curcol);

#if ENABLE_EXTRA_CHECKS
	if (IS_TDS7_PLUS(tds))
		tdsdump_log(TDS_DBG_ERROR, "tds_put_data_info_length called with TDS7+\n");
#endif

	/* TODO ICONV convert string if needed (see also tds_put_data_info) */
	if (flags & TDS_PUT_DATA_USE_NAME)
		len += curcol->column_namelen;
	if (is_numeric_type(curcol->on_server.column_type))
		len += 2;
	return len + curcol->column_varint_size;
}

/**
 * Write data to wire
 * \param tds     state information for the socket and the TDS protocol
 * \param curcol  column where store column information
 * \return TDS_FAIL on error or TDS_SUCCEED
 */
static int
tds_put_data(TDSSOCKET * tds, TDSCOLUMN * curcol)
{
	unsigned char *src;
	TDS_NUMERIC *num;
	TDSBLOB *blob = NULL;
	TDS_INT colsize;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(curcol);

	colsize = curcol->column_cur_size;
	src = curcol->column_data;

	tdsdump_log(TDS_DBG_INFO1, "tds_put_data: colsize = %d\n", (int) colsize);

	if (colsize < 0) {
		tdsdump_log(TDS_DBG_INFO1, "tds_put_data: null param\n");
		switch (curcol->column_varint_size) {
		case 4:
			tds_put_int(tds, -1);
			break;
		case 2:
			tds_put_smallint(tds, -1);
			break;
		default:
			assert(curcol->column_varint_size);
			/* FIXME not good for SYBLONGBINARY/SYBLONGCHAR (still not supported) */
			tds_put_byte(tds, 0);
			break;
		}
		return TDS_SUCCEED;
	}

	/*
	 * TODO here we limit data sent with MIN, should mark somewhere
	 * and inform client ??
	 * Test proprietary behavior
	 */
	if (IS_TDS7_PLUS(tds)) {
		const char *s;
		int converted = 0;

		tdsdump_log(TDS_DBG_INFO1, "tds_put_data: not null param varint_size = %d\n",
			    curcol->column_varint_size);

		if (is_blob_type(curcol->column_type)) {
			blob = (TDSBLOB *) src;
			src = (unsigned char *) blob->textvalue;
		}
		s = (char *) src;

		/* convert string if needed */
		if (curcol->char_conv && curcol->char_conv->flags != TDS_ENCODING_MEMCPY) {
#if 0
			/* TODO this case should be optimized */
			/* we know converted bytes */
			if (curcol->char_conv->client_charset.min_bytes_per_char == curcol->char_conv->client_charset.max_bytes_per_char 
			    && curcol->char_conv->server_charset.min_bytes_per_char == curcol->char_conv->server_charset.max_bytes_per_char) {
				converted_size = colsize * curcol->char_conv->server_charset.min_bytes_per_char / curcol->char_conv->client_charset.min_bytes_per_char;

			} else {
#endif
			/* we need to convert data before */
			/* TODO this can be a waste of memory... */
			converted = 1;
			s = tds_convert_string(tds, curcol->char_conv, s, colsize, &colsize);
			if (!s) {
				/* on conversion error put a empty string */
				/* TODO on memory failure we should compute comverted size and use chunks */
				colsize = 0;
				converted = -1;
			}
		}

		switch (curcol->column_varint_size) {
		case 4:	/* It's a BLOB... */
			blob = (TDSBLOB *) curcol->column_data;
			colsize = MIN(colsize, 0x7fffffff);
			/* mssql require only size */
			tds_put_int(tds, colsize);
			break;
		case 2:
			colsize = MIN(colsize, 8000);
			tds_put_smallint(tds, colsize);
			break;
		case 1:
			if (is_numeric_type(curcol->on_server.column_type))
				colsize = tds_numeric_bytes_per_prec[((TDS_NUMERIC *) src)->precision];
			colsize = MIN(colsize, 255);
			tds_put_byte(tds, colsize);
			break;
		case 0:
			/* TODO should be column_size */
			colsize = tds_get_size_by_type(curcol->on_server.column_type);
			break;
		}

		/* conversion error, exit with an error */
		if (converted < 0)
			return TDS_FAIL;

		/* put real data */
		if (is_numeric_type(curcol->on_server.column_type)) {
			TDS_NUMERIC buf;

			memcpy(&buf, src, sizeof(buf));
			tdsdump_log(TDS_DBG_INFO1, "swapping numeric data...\n");
			tds_swap_numeric(&buf);
			tds_put_n(tds, buf.array, colsize);
		} else if (blob) {
			tds_put_n(tds, s, colsize);
		} else {
#ifdef WORDS_BIGENDIAN
			unsigned char buf[64];

			if (tds->emul_little_endian && !converted && colsize < 64) {
				tdsdump_log(TDS_DBG_INFO1, "swapping coltype %d\n",
					    tds_get_conversion_type(curcol->column_type, colsize));
				memcpy(buf, s, colsize);
				tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), buf);
				s = (char *) buf;
			}
#endif
			tds_put_n(tds, s, colsize);
		}
		if (converted)
			tds_convert_string_free((char*)src, s);
	} else {
		/* TODO ICONV handle charset conversions for data */
		/* put size of data */
		switch (curcol->column_varint_size) {
		case 4:	/* It's a BLOB... */
			blob = (TDSBLOB *) curcol->column_data;
			tds_put_byte(tds, 16);
			tds_put_n(tds, blob->textptr, 16);
			tds_put_n(tds, blob->timestamp, 8);
			colsize = MIN(colsize, 0x7fffffff);
			tds_put_int(tds, colsize);
			break;
		case 2:
			colsize = MIN(colsize, 8000);
			tds_put_smallint(tds, colsize);
			break;
		case 1:
			if (is_numeric_type(curcol->column_type))
				colsize = tds_numeric_bytes_per_prec[((TDS_NUMERIC *) src)->precision];
			colsize = MIN(colsize, 255);
			tds_put_byte(tds, colsize);
			break;
		case 0:
			/* TODO should be column_size */
			colsize = tds_get_size_by_type(curcol->column_type);
			break;
		}

		/* put real data */
		if (is_numeric_type(curcol->column_type)) {
			num = (TDS_NUMERIC *) src;
			tds_put_n(tds, num->array, colsize);
		} else if (is_blob_type(curcol->column_type)) {
			blob = (TDSBLOB *) src;
			/* FIXME ICONV handle conversion when needed */
			tds_put_n(tds, blob->textvalue, colsize);
		} else {
#ifdef WORDS_BIGENDIAN
			unsigned char buf[64];

			if (tds->emul_little_endian && colsize < 64) {
				tdsdump_log(TDS_DBG_INFO1, "swapping coltype %d\n",
					    tds_get_conversion_type(curcol->column_type, colsize));
				memcpy(buf, src, colsize);
				tds_swap_datatype(tds_get_conversion_type(curcol->column_type, colsize), buf);
				src = buf;
			}
#endif
			tds_put_n(tds, src, colsize);
		}
	}
	return TDS_SUCCEED;
}

static void
tds7_send_execute(TDSSOCKET * tds, TDSDYNAMIC * dyn)
{
	TDSCOLUMN *param;
	TDSPARAMINFO *info;
	int i;

	/* procedure name */
	tds_put_smallint(tds, 10);
	/* NOTE do not call this procedure using integer name (TDS_SP_EXECUTE) on mssql2k, it doesn't work! */
	TDS_PUT_N_AS_UCS2(tds, "sp_execute");
	tds_put_smallint(tds, 0);	/* flags */

	/* id of prepared statement */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	tds_put_byte(tds, SYBINTN);
	tds_put_byte(tds, 4);
	tds_put_byte(tds, 4);
	tds_put_int(tds, dyn->num_id);

	info = dyn->params;
	if (info)
		for (i = 0; i < info->num_cols; i++) {
			param = info->columns[i];
			/* TODO check error */
			tds_put_data_info(tds, param, 0);
			/* FIXME handle error */
			tds_put_data(tds, param);
		}

	tds->internal_sp_called = TDS_SP_EXECUTE;
}

/**
 * tds_submit_execute() sends a previously prepared dynamic statement to the 
 * server.
 * \param tds state information for the socket and the TDS protocol
 * \param dyn dynamic proc to execute. Must build from same tds.
 */
int
tds_submit_execute(TDSSOCKET * tds, TDSDYNAMIC * dyn)
{
	int id_len;

	CHECK_TDS_EXTRA(tds);
	/* TODO this dynamic should be in tds */
	CHECK_DYNAMIC_EXTRA(dyn);

	tdsdump_log(TDS_DBG_FUNC, "tds_submit_execute()\n");

	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		return TDS_FAIL;

	tds->cur_dyn = dyn;

	if (IS_TDS7_PLUS(tds)) {
		/* check proper id */
		if (dyn->num_id == 0) {
			tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}

		/* RPC on sp_execute */
		tds->out_flag = TDS_RPC;
		START_QUERY;

		tds7_send_execute(tds, dyn);

		return tds_query_flush_packet(tds);
	}

	if (dyn->emulated) {
		if (tds_send_emulated_execute(tds, dyn->query, dyn->params) != TDS_SUCCEED)
			return TDS_FAIL;
		return tds_query_flush_packet(tds);
	}

	/* query has been prepared successfully, discard original query */
	if (dyn->query)
		TDS_ZERO_FREE(dyn->query);

	tds->out_flag = TDS_NORMAL;
	/* dynamic id */
	id_len = strlen(dyn->id);

	tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
	tds_put_smallint(tds, id_len + 5);
	tds_put_byte(tds, 0x02);
	tds_put_byte(tds, dyn->params ? 0x01 : 0);
	tds_put_byte(tds, id_len);
	tds_put_n(tds, dyn->id, id_len);
	tds_put_smallint(tds, 0);

	if (dyn->params)
		tds_put_params(tds, dyn->params, 0);

	/* send it */
	return tds_query_flush_packet(tds);
}

static void
tds_put_params(TDSSOCKET * tds, TDSPARAMINFO * info, int flags)
{
	int i, len;

	CHECK_TDS_EXTRA(tds);
	CHECK_PARAMINFO_EXTRA(info);

	/* column descriptions */
	tds_put_byte(tds, TDS5_PARAMFMT_TOKEN);
	/* size */
	len = 2;
	for (i = 0; i < info->num_cols; i++)
		len += tds_put_data_info_length(tds, info->columns[i], flags);
	tds_put_smallint(tds, len);
	/* number of parameters */
	tds_put_smallint(tds, info->num_cols);
	/* column detail for each parameter */
	for (i = 0; i < info->num_cols; i++) {
		/* FIXME add error handling */
		tds_put_data_info(tds, info->columns[i], flags);
	}

	/* row data */
	tds_put_byte(tds, TDS5_PARAMS_TOKEN);
	for (i = 0; i < info->num_cols; i++) {
		/* FIXME handle error */
		tds_put_data(tds, info->columns[i]);
	}
}

/**
 * Send a unprepare request for a prepared query
 * \param tds state information for the socket and the TDS protocol
 * \param dyn dynamic query
 * \result TDS_SUCCEED or TDS_FAIL
 */
int
tds_submit_unprepare(TDSSOCKET * tds, TDSDYNAMIC * dyn)
{
	int id_len;

	CHECK_TDS_EXTRA(tds);
	/* TODO test dyn in tds */
	CHECK_DYNAMIC_EXTRA(dyn);

	if (!dyn)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_FUNC, "tds_submit_unprepare() %s\n", dyn->id);

	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		return TDS_FAIL;

	tds->cur_dyn = dyn;

	if (IS_TDS7_PLUS(tds)) {
		/* RPC on sp_execute */
		tds->out_flag = TDS_RPC;
		START_QUERY;
		/* procedure name */
		if (IS_TDS8_PLUS(tds)) {
			/* save some byte for mssql2k */
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_UNPREPARE);
		} else {
			tds_put_smallint(tds, 12);
			TDS_PUT_N_AS_UCS2(tds, "sp_unprepare");
		}
		tds_put_smallint(tds, 0);	/* flags */

		/* id of prepared statement */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 0);
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, dyn->num_id);

		tds->internal_sp_called = TDS_SP_UNPREPARE;
		return tds_query_flush_packet(tds);
	}

	if (dyn->emulated) {
		tds->out_flag = TDS_QUERY;
		START_QUERY;
		/* just a dummy select to return some data */
		tds_put_string(tds, "select 1 where 0=1", -1);
		return tds_query_flush_packet(tds);
	}

	tds->out_flag = TDS_NORMAL;
	/* dynamic id */
	id_len = strlen(dyn->id);

	tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
	tds_put_smallint(tds, id_len + 5);
	tds_put_byte(tds, 0x04);
	tds_put_byte(tds, 0x00);
	tds_put_byte(tds, id_len);
	tds_put_n(tds, dyn->id, id_len);
	tds_put_smallint(tds, 0);

	/* send it */
	return tds_query_flush_packet(tds);
}

/**
 * tds_submit_rpc() call a RPC from server. Output parameters will be stored in tds->param_info
 * \param tds      state information for the socket and the TDS protocol
 * \param rpc_name name of RPC
 * \param params   parameters informations. NULL for no parameters
 */
int
tds_submit_rpc(TDSSOCKET * tds, const char *rpc_name, TDSPARAMINFO * params)
{
	TDSCOLUMN *param;
	int rpc_name_len, i;
	int num_params = params ? params->num_cols : 0;

	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);

	assert(tds);
	assert(rpc_name);

	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		return TDS_FAIL;

	/* distinguish from dynamic query  */
	tds->cur_dyn = NULL;

	rpc_name_len = strlen(rpc_name);
	if (IS_TDS7_PLUS(tds)) {
		const char *converted_name;
		int converted_name_len;

		tds->out_flag = TDS_RPC;
		/* procedure name */
		converted_name = tds_convert_string(tds, tds->char_convs[client2ucs2], rpc_name, rpc_name_len, &converted_name_len);
		if (!converted_name) {
			tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}
		START_QUERY;
		tds_put_smallint(tds, converted_name_len / 2);
		tds_put_n(tds, converted_name, converted_name_len);
		tds_convert_string_free(rpc_name, converted_name);

		/*
		 * TODO support flags
		 * bit 0 (1 as flag) in TDS7/TDS5 is "recompile"
		 * bit 1 (2 as flag) in TDS7+ is "no metadata" bit 
		 * (I don't know meaning of "no metadata")
		 */
		tds_put_smallint(tds, 0);

		for (i = 0; i < num_params; i++) {
			param = params->columns[i];
			/* TODO check error */
			tds_put_data_info(tds, param, TDS_PUT_DATA_USE_NAME);
			/* FIXME handle error */
			tds_put_data(tds, param);
		}

		return tds_query_flush_packet(tds);
	}

	if (IS_TDS50(tds)) {
		tds->out_flag = TDS_NORMAL;

		/* DBRPC */
		tds_put_byte(tds, TDS_DBRPC_TOKEN);
		/* TODO ICONV convert rpc name */
		tds_put_smallint(tds, rpc_name_len + 3);
		tds_put_byte(tds, rpc_name_len);
		tds_put_n(tds, rpc_name, rpc_name_len);
		/* TODO flags */
		tds_put_smallint(tds, num_params ? 2 : 0);

		if (num_params)
			tds_put_params(tds, params, TDS_PUT_DATA_USE_NAME);

		/* send it */
		return tds_query_flush_packet(tds);
	}

	/* TODO emulate it for TDS4.x, send RPC for mssql */
	/* TODO continue, support for TDS4?? */
	tds_set_state(tds, TDS_IDLE);
	return TDS_FAIL;
}

/**
 * tds_send_cancel() sends an empty packet (8 byte header only)
 * tds_process_cancel should be called directly after this.
 * \param tds state information for the socket and the TDS protocol
 * \remarks
 *	tcp will either deliver the packet or time out. 
 *	(TIME_WAIT determines how long it waits between retries.)  
 *	
 *	On sending the cancel, we may get EAGAIN.  We then select(2) until we know
 *	either 1) it succeeded or 2) it didn't.  On failure, close the socket,
 *	tell the app, and fail the function.  
 *	
 *	On success, we read(2) and wait for a reply with select(2).  If we get
 *	one, great.  If the client's timeout expires, we tell him, but all we can
 *	do is wait some more or give up and close the connection.  If he tells us
 *	to cancel again, we wait some more.  
 */
int
tds_send_cancel(TDSSOCKET * tds)
{
	CHECK_TDS_EXTRA(tds);

 	tdsdump_log(TDS_DBG_FUNC, "tds_send_cancel: %sin_cancel and %sidle\n", 
				(tds->in_cancel? "":"not "), (tds->state == TDS_IDLE? "":"not "));
	
	/* one cancel is sufficient */
	if (tds->in_cancel || tds->state == TDS_IDLE)
		return TDS_SUCCEED;

	tds->out_flag = TDS_CANCEL;
	tds->in_cancel = 1;
 	tdsdump_log(TDS_DBG_FUNC, "tds_send_cancel: sending cancel packet\n");
	return tds_flush_packet(tds);
}

static int
tds_quote(TDSSOCKET * tds, char *buffer, char quoting, const char *id, int len)
{
	int i;
	const char *src, *pend;
	char *dst;

	CHECK_TDS_EXTRA(tds);

	pend = id + len;

	/* quote */
	src = id;
	if (!buffer) {
		i = 2 + len;
		for (; src != pend; ++src)
			if (*src == quoting)
				++i;
		return i;
	}

	dst = buffer;
	*dst++ = (quoting == ']') ? '[' : quoting;
	for (; src != pend; ++src) {
		if (*src == quoting)
			*dst++ = quoting;
		*dst++ = *src;
	}
	*dst++ = quoting;
	*dst = 0;
	return dst - buffer;
}

/**
 * Quote an id
 * \param tds    state information for the socket and the TDS protocol
 * \param buffer buffer to store quoted id. If NULL do not write anything 
 *        (useful to compute quote length)
 * \param id     id to quote
 * \param idlen  id length
 * \result written chars (not including needed terminator)
 */
int
tds_quote_id(TDSSOCKET * tds, char *buffer, const char *id, int idlen)
{
	int i;

	CHECK_TDS_EXTRA(tds);

	if (idlen < 0)
		idlen = strlen(id);

	/* need quote ?? */
	for (i = 0; i < idlen; ++i) {
		char c = id[i];

		if (c >= 'a' && c <= 'z')
			continue;
		if (c >= 'A' && c <= 'Z')
			continue;
		if (i > 0 && c >= '0' && c <= '9')
			continue;
		if (c == '_')
			continue;
		return tds_quote(tds, buffer, TDS_IS_MSSQL(tds) ? ']' : '\"', id, idlen);
	}

	if (buffer) {
		memcpy(buffer, id, idlen);
		buffer[idlen] = '\0';
	}
	return idlen;
}

/**
 * Quote a string
 * \param tds    state information for the socket and the TDS protocol
 * \param buffer buffer to store quoted id. If NULL do not write anything 
 *        (useful to compute quote length)
 * \param str    string to quote (not necessary null-terminated)
 * \param len    length of string (-1 for null terminated)
 * \result written chars (not including needed terminator)
 */
int
tds_quote_string(TDSSOCKET * tds, char *buffer, const char *str, int len)
{
	return tds_quote(tds, buffer, '\'', str, len < 0 ? strlen(str) : len);
}

static inline void
tds_set_cur_cursor(TDSSOCKET *tds, TDSCURSOR *cursor)
{
	++cursor->ref_count;
	if (tds->cur_cursor)
		tds_release_cursor(tds, tds->cur_cursor);
	tds->cur_cursor = cursor;
}

int
tds_cursor_declare(TDSSOCKET * tds, TDSCURSOR * cursor, TDSPARAMINFO *params, int *something_to_send)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_declare() cursor id = %d\n", cursor->cursor_id);

	if (IS_TDS7_PLUS(tds)) {
		cursor->srv_status |= TDS_CUR_ISTAT_DECLARED;
		cursor->srv_status |= TDS_CUR_ISTAT_CLOSED;
		cursor->srv_status |= TDS_CUR_ISTAT_RDONLY;
	}

	if (IS_TDS50(tds)) {
		if (!*something_to_send) {
			if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
				return TDS_FAIL;

			tds->out_flag = TDS_NORMAL;
		}
		if (tds->state != TDS_QUERYING || tds->out_flag != TDS_NORMAL)
			return TDS_FAIL;

		tds_put_byte(tds, TDS_CURDECLARE_TOKEN);

		/* length of the data stream that follows */
		tds_put_smallint(tds, (6 + strlen(cursor->cursor_name) + strlen(cursor->query)));

		tdsdump_log(TDS_DBG_ERROR, "size = %u\n", (unsigned int) (6u + strlen(cursor->cursor_name) + strlen(cursor->query)));

		tds_put_tinyint(tds, strlen(cursor->cursor_name));
		tds_put_n(tds, cursor->cursor_name, strlen(cursor->cursor_name));
		tds_put_byte(tds, 1);	/* cursor option is read only=1, unused=0 */
		tds_put_byte(tds, 0);	/* status unused=0 */
		/* TODO iconv */
		tds_put_smallint(tds, strlen(cursor->query));
		tds_put_n(tds, cursor->query, strlen(cursor->query));
		tds_put_tinyint(tds, 0);	/* number of columns = 0 , valid value applicable only for updatable cursor */
		*something_to_send = 1;
	}

	return TDS_SUCCEED;
}

int
tds_cursor_open(TDSSOCKET * tds, TDSCURSOR * cursor, TDSPARAMINFO *params, int *something_to_send)
{
	int converted_query_len;
	const char *converted_query;

	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_open() cursor id = %d\n", cursor->cursor_id);

	if (!*something_to_send) {
		if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
			return TDS_FAIL;
	}
	if (tds->state != TDS_QUERYING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	if (IS_TDS50(tds)) {

		tds->out_flag = TDS_NORMAL;
		tds_put_byte(tds, TDS_CUROPEN_TOKEN);
		tds_put_smallint(tds, 6 + strlen(cursor->cursor_name));	/* length of the data stream that follows */

		/*tds_put_int(tds, cursor->cursor_id); *//* Only if cursor id is passed as zero, the cursor name need to be sent */

		tds_put_int(tds, 0);
		tds_put_tinyint(tds, strlen(cursor->cursor_name));
		tds_put_n(tds, cursor->cursor_name, strlen(cursor->cursor_name));
		tds_put_byte(tds, 0);	/* Cursor status : 0 for no arguments */
		*something_to_send = 1;
	}
	if (IS_TDS7_PLUS(tds)) {
		size_t definition_len = 0;
		char *param_definition = NULL;
		int num_params = params ? params->num_cols : 0;

		/* cursor statement */
		converted_query = tds_convert_string(tds, tds->char_convs[client2ucs2],
						     cursor->query, strlen(cursor->query), &converted_query_len);
		if (!converted_query) {
			if (!*something_to_send)
				tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}

		if (num_params) {
			param_definition = tds7_build_param_def_from_query(tds, converted_query, converted_query_len, params, &definition_len);
			if (!param_definition) {
				tds_convert_string_free(cursor->query, converted_query);
				if (!*something_to_send)
					tds_set_state(tds, TDS_IDLE);
				return TDS_FAIL;
			}
		}

		/* RPC call to sp_cursoropen */

		tds->out_flag = TDS_RPC;
		START_QUERY;

		/* procedure identifier by number */

		if (IS_TDS8_PLUS(tds)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_CURSOROPEN);
		} else {
			tds_put_smallint(tds, 13);
			TDS_PUT_N_AS_UCS2(tds, "sp_cursoropen");
		}

		tds_put_smallint(tds, 0);	/* flags */

		/* return cursor handle (int) */

		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 1);	/* output parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 0);

		if (definition_len) {
			tds7_put_query_params(tds, converted_query, converted_query_len);
		} else {
			tds_put_byte(tds, 0);
			tds_put_byte(tds, 0);
			tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
			tds_put_int(tds, converted_query_len);
			if (IS_TDS8_PLUS(tds))
				tds_put_n(tds, tds->collation, 5);
			tds_put_int(tds, converted_query_len);
			tds_put_n(tds, converted_query, converted_query_len);
		}
		tds_convert_string_free(cursor->query, converted_query);

		/* type */
		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 1);	/* output parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, definition_len ? cursor->type | 0x1000 : cursor->type);

		/* concurrency */
		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 1);	/* output parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, cursor->concurrency);

		/* row count */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 1);	/* output parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, 0);

		if (definition_len) {
			int i;

			tds7_put_params_definition(tds, param_definition, definition_len);

			for (i = 0; i < num_params; i++) {
				TDSCOLUMN *param = params->columns[i];
				/* TODO check error */
				tds_put_data_info(tds, param, 0);
				/* FIXME handle error */
				tds_put_data(tds, param);
			}
		}
		free(param_definition);

		*something_to_send = 1;
		tds->internal_sp_called = TDS_SP_CURSOROPEN;
		tdsdump_log(TDS_DBG_ERROR, "tds_cursor_open (): RPC call set up \n");
	}


	tdsdump_log(TDS_DBG_ERROR, "tds_cursor_open (): cursor open completed\n");
	return TDS_SUCCEED;
}

int
tds_cursor_setrows(TDSSOCKET * tds, TDSCURSOR * cursor, int *something_to_send)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_setrows() cursor id = %d\n", cursor->cursor_id);

	if (IS_TDS7_PLUS(tds)) {
		cursor->srv_status &= ~TDS_CUR_ISTAT_DECLARED;
		cursor->srv_status |= TDS_CUR_ISTAT_CLOSED;
		cursor->srv_status |= TDS_CUR_ISTAT_ROWCNT;
	}

	if (IS_TDS50(tds)) {
		if (!*something_to_send) {
			if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
				return TDS_FAIL;

			tds->out_flag = TDS_NORMAL;
		}
		if (tds->state != TDS_QUERYING  || tds->out_flag != TDS_NORMAL)
			return TDS_FAIL;

		tds_set_cur_cursor(tds, cursor);
		tds_put_byte(tds, TDS_CURINFO_TOKEN);

		tds_put_smallint(tds, 12 + strlen(cursor->cursor_name));
		/* length of data stream that follows */

		/* tds_put_int(tds, tds->cursor->cursor_id); */ /* Cursor id */

		tds_put_int(tds, 0);
		tds_put_tinyint(tds, strlen(cursor->cursor_name));
		tds_put_n(tds, cursor->cursor_name, strlen(cursor->cursor_name));
		tds_put_byte(tds, 1);	/* Command  TDS_CUR_CMD_SETCURROWS */
		tds_put_byte(tds, 0x00);	/* Status - TDS_CUR_ISTAT_ROWCNT 0x0020 */
		tds_put_byte(tds, 0x20);	/* Status - TDS_CUR_ISTAT_ROWCNT 0x0020 */
		tds_put_int(tds, cursor->cursor_rows);	/* row count to set */
		*something_to_send = 1;

	}
	return TDS_SUCCEED;
}

static void
tds7_put_cursor_fetch(TDSSOCKET * tds, TDS_INT cursor_id, TDS_TINYINT fetch_type, TDS_INT i_row, TDS_INT num_rows)
{
	if (IS_TDS8_PLUS(tds)) {
		tds_put_smallint(tds, -1);
		tds_put_smallint(tds, TDS_SP_CURSORFETCH);
	} else {
		tds_put_smallint(tds, 14);
		TDS_PUT_N_AS_UCS2(tds, "sp_cursorfetch");
	}

	/* This flag tells the SP only to */
	/* output a dummy metadata token  */

	tds_put_smallint(tds, 2);

	/* input cursor handle (int) */

	tds_put_byte(tds, 0);	/* no parameter name */
	tds_put_byte(tds, 0);	/* input parameter  */
	tds_put_byte(tds, SYBINTN);
	tds_put_byte(tds, 4);
	tds_put_byte(tds, 4);
	tds_put_int(tds, cursor_id);

	/* fetch type - 2 = NEXT */

	tds_put_byte(tds, 0);	/* no parameter name */
	tds_put_byte(tds, 0);	/* input parameter  */
	tds_put_byte(tds, SYBINTN);
	tds_put_byte(tds, 4);
	tds_put_byte(tds, 4);
	tds_put_int(tds, fetch_type);

	/* row number */
	tds_put_byte(tds, 0);	/* no parameter name */
	tds_put_byte(tds, 0);	/* input parameter  */
	tds_put_byte(tds, SYBINTN);
	tds_put_byte(tds, 4);
	if ((fetch_type & 0x30) != 0) {
		tds_put_byte(tds, 4);
		tds_put_int(tds, i_row);
	} else {
		tds_put_byte(tds, 0);
	}

	/* number of rows to fetch */
	tds_put_byte(tds, 0);	/* no parameter name */
	tds_put_byte(tds, 0);	/* input parameter  */
	tds_put_byte(tds, SYBINTN);
	tds_put_byte(tds, 4);
	tds_put_byte(tds, 4);
	tds_put_int(tds, num_rows);
}

int
tds_cursor_fetch(TDSSOCKET * tds, TDSCURSOR * cursor, TDS_CURSOR_FETCH fetch_type, TDS_INT i_row)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_fetch() cursor id = %d\n", cursor->cursor_id);

	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	if (IS_TDS50(tds)) {
		size_t len = strlen(cursor->cursor_name);
		size_t row_len = 0;

		tds->out_flag = TDS_NORMAL;
		tds_put_byte(tds, TDS_CURFETCH_TOKEN);

		if (len > (255-10))
			len = (255-10);
		if (fetch_type == TDS_CURSOR_FETCH_ABSOLUTE || fetch_type == TDS_CURSOR_FETCH_RELATIVE)
			row_len = 4;

		/*tds_put_smallint(tds, 8); */

		tds_put_smallint(tds, 6 + len + row_len);	/* length of the data stream that follows */

		/*tds_put_int(tds, cursor->cursor_id); *//* cursor id returned by the server */

		tds_put_int(tds, 0);
		tds_put_tinyint(tds, len);
		tds_put_n(tds, cursor->cursor_name, len);
		tds_put_tinyint(tds, fetch_type);

		/* optional argument to fetch row at absolute/relative position */
		if (row_len)
			tds_put_int(tds, i_row);
		return tds_query_flush_packet(tds);
	}

	if (IS_TDS7_PLUS(tds)) {

		/* RPC call to sp_cursorfetch */
		static const unsigned char mssql_fetch[7] = {
			0,
			2,    /* TDS_CURSOR_FETCH_NEXT */
			4,    /* TDS_CURSOR_FETCH_PREV */
			1,    /* TDS_CURSOR_FETCH_FIRST */
			8,    /* TDS_CURSOR_FETCH_LAST */
			0x10, /* TDS_CURSOR_FETCH_ABSOLUTE */
			0x20  /* TDS_CURSOR_FETCH_RELATIVE */
		};

		tds->out_flag = TDS_RPC;
		START_QUERY;

		/* TODO enum for 2 ... */
		if (cursor->type == 2 && fetch_type == TDS_CURSOR_FETCH_ABSOLUTE) {
			/* strangely dynamic cursor do not support absolute so emulate it with first + relative */
			tds7_put_cursor_fetch(tds, cursor->cursor_id, 1, 0, 0);
			/* TODO define constant */
			tds_put_byte(tds, IS_TDS90(tds) ? 0xff : 0x80);
			tds7_put_cursor_fetch(tds, cursor->cursor_id, 0x20, i_row, cursor->cursor_rows);
		} else {
			/* TODO check fetch_type ?? */
			tds7_put_cursor_fetch(tds, cursor->cursor_id, mssql_fetch[fetch_type], i_row, cursor->cursor_rows);
		}

		tds->internal_sp_called = TDS_SP_CURSORFETCH;
		return tds_query_flush_packet(tds);
	}

	tds_set_state(tds, TDS_IDLE);
	return TDS_SUCCEED;
}

int
tds_cursor_close(TDSSOCKET * tds, TDSCURSOR * cursor)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_close() cursor id = %d\n", cursor->cursor_id);

	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	if (IS_TDS50(tds)) {
		tds->out_flag = TDS_NORMAL;
		tds_put_byte(tds, TDS_CURCLOSE_TOKEN);
		tds_put_smallint(tds, 5);	/* length of the data stream that follows */
		tds_put_int(tds, cursor->cursor_id);	/* cursor id returned by the server is available now */

		if (cursor->status.dealloc == TDS_CURSOR_STATE_REQUESTED) {
			tds_put_byte(tds, 0x01);	/* Close option: TDS_CUR_COPT_DEALLOC */
			cursor->status.dealloc = TDS_CURSOR_STATE_SENT;
		}
		else
			tds_put_byte(tds, 0x00);	/* Close option: TDS_CUR_COPT_UNUSED */

	}
	if (IS_TDS7_PLUS(tds)) {

		/* RPC call to sp_cursorclose */

		tds->out_flag = TDS_RPC;
		START_QUERY;

		if (IS_TDS8_PLUS(tds)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_CURSORCLOSE);
		} else {
			tds_put_smallint(tds, 14);
			TDS_PUT_N_AS_UCS2(tds, "sp_cursorclose");
		}

		/* This flag tells the SP to output only a dummy metadata token  */

		tds_put_smallint(tds, 2);

		/* input cursor handle (int) */

		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 0);	/* input parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, cursor->cursor_id);
		tds->internal_sp_called = TDS_SP_CURSORCLOSE;
	}
	return tds_query_flush_packet(tds);

}

int
tds_cursor_setname(TDSSOCKET * tds, TDSCURSOR * cursor)
{
	int len;

	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_setname() cursor id = %d\n", cursor->cursor_id);

	if (!IS_TDS7_PLUS(tds))
		return TDS_SUCCEED;

	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	/* RPC call to sp_cursoroption */
	tds->out_flag = TDS_RPC;
	START_QUERY;

	if (IS_TDS8_PLUS(tds)) {
		tds_put_smallint(tds, -1);
		tds_put_smallint(tds, TDS_SP_CURSOROPTION);
	} else {
		tds_put_smallint(tds, 14);
		TDS_PUT_N_AS_UCS2(tds, "sp_cursoroption");
	}

	tds_put_smallint(tds, 0);

	/* input cursor handle (int) */
	tds_put_byte(tds, 0);	/* no parameter name */
	tds_put_byte(tds, 0);	/* input parameter  */
	tds_put_byte(tds, SYBINTN);
	tds_put_byte(tds, 4);
	tds_put_byte(tds, 4);
	tds_put_int(tds, cursor->cursor_id);

	/* code, 2 == set cursor name */
	tds_put_byte(tds, 0);	/* no parameter name */
	tds_put_byte(tds, 0);	/* input parameter  */
	tds_put_byte(tds, SYBINTN);
	tds_put_byte(tds, 4);
	tds_put_byte(tds, 4);
	tds_put_int(tds, 2);

	/* cursor name */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	/* TODO convert ?? */
	tds_put_byte(tds, XSYBVARCHAR);
	len = strlen(cursor->cursor_name);
	tds_put_smallint(tds, len);
	if (IS_TDS8_PLUS(tds))
		tds_put_n(tds, tds->collation, 5);
	tds_put_smallint(tds, len);
	tds_put_n(tds, cursor->cursor_name, len);

	tds->internal_sp_called = TDS_SP_CURSOROPTION;
	return tds_query_flush_packet(tds);

}

int 
tds_cursor_update(TDSSOCKET * tds, TDSCURSOR * cursor, TDS_CURSOR_OPERATION op, TDS_INT i_row, TDSPARAMINFO *params)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_update() cursor id = %d\n", cursor->cursor_id);

	/* client must provide parameters for update */
	if (op == TDS_CURSOR_UPDATE && (!params || params->num_cols <= 0))
		return TDS_FAIL;

	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	if (IS_TDS50(tds)) {
		tds->out_flag = TDS_NORMAL;

		/* FIXME finish*/
		tds_set_state(tds, TDS_IDLE);
		return TDS_FAIL;
	}
	if (IS_TDS7_PLUS(tds)) {

		/* RPC call to sp_cursorclose */

		tds->out_flag = TDS_RPC;
		START_QUERY;

		if (IS_TDS8_PLUS(tds)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_CURSOR);
		} else {
			tds_put_smallint(tds, 14);
			TDS_PUT_N_AS_UCS2(tds, "sp_cursor");
		}

		tds_put_smallint(tds, 0);

		/* input cursor handle (int) */
		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 0);	/* input parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, cursor->cursor_id);

		/* cursor operation */
		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 0);	/* input parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, 32 | op);

		/* row number */
		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 0);	/* input parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, i_row);

		/* update require table name */
		if (op == TDS_CURSOR_UPDATE) {
			TDSCOLUMN *param;
			unsigned int n, num_params;
			const char *table_name = NULL;
			int converted_table_len = 0;
			const char *converted_table = NULL;

			/* empty table name */
			tds_put_byte(tds, 0);
			tds_put_byte(tds, 0);
			tds_put_byte(tds, XSYBNVARCHAR);
			num_params = params->num_cols;
			for (n = 0; n < num_params; ++n) {
				param = params->columns[n];
				if (param->table_namelen > 0) {
					table_name = param->table_name;
					break;
				}
			}
			if (table_name) {
				converted_table =
					tds_convert_string(tds, tds->char_convs[client2ucs2], 
							   table_name, strlen(table_name), &converted_table_len);
				if (!converted_table) {
					/* FIXME not here, in the middle of a packet */
					tds_set_state(tds, TDS_IDLE);
					return TDS_FAIL;
				}
			}
			tds_put_smallint(tds, converted_table_len);
			if (IS_TDS8_PLUS(tds))
				tds_put_n(tds, tds->collation, 5);
			tds_put_smallint(tds, converted_table_len);
			tds_put_n(tds, converted_table, converted_table_len);
			tds_convert_string_free(table_name, converted_table);

			/* output columns to update */
			for (n = 0; n < num_params; ++n) {
				param = params->columns[n];
				/* TODO check error */
				tds_put_data_info(tds, param, TDS_PUT_DATA_USE_NAME|TDS_PUT_DATA_PREFIX_NAME);
				/* FIXME handle error */
				tds_put_data(tds, param);
			}
		}

		tds->internal_sp_called = TDS_SP_CURSOR;
	}
	return tds_query_flush_packet(tds);
}

/**
 * Send a deallocation request to server
 * libTDS care for all deallocation stuff (memory and server cursor)
 * Caller should not use cursor pointer anymore
 */
int
tds_cursor_dealloc(TDSSOCKET * tds, TDSCURSOR * cursor)
{
	int res = TDS_SUCCEED;

	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	if (cursor->srv_status == TDS_CUR_ISTAT_UNUSED || (cursor->srv_status & TDS_CUR_ISTAT_DEALLOC) != 0 
	    || (IS_TDS7_PLUS(tds) && (cursor->srv_status & TDS_CUR_ISTAT_CLOSED) != 0)) {
		tds_cursor_deallocated(tds, cursor);
		tds_release_cursor(tds, cursor);
		return TDS_SUCCEED;
	}

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_dealloc() cursor id = %d\n", cursor->cursor_id);

	if (IS_TDS50(tds)) {
		if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
			return TDS_FAIL;
		tds_set_cur_cursor(tds, cursor);

		tds->out_flag = TDS_NORMAL;
		tds_put_byte(tds, TDS_CURCLOSE_TOKEN);
		tds_put_smallint(tds, 5);	/* length of the data stream that follows */
		tds_put_int(tds, cursor->cursor_id);	/* cursor id returned by the server is available now */
		tds_put_byte(tds, 0x01);	/* Close option: TDS_CUR_COPT_DEALLOC */
		res = tds_query_flush_packet(tds);
	}

	/*
	 * in TDS 5 the cursor deallocate function involves
	 * a server interaction. The cursor will be freed
	 * when we receive acknowledgement of the cursor
	 * deallocate from the server. for TDS 7 we do it
	 * here...
	 */
	if (IS_TDS7_PLUS(tds)) {
		if (cursor->status.dealloc == TDS_CURSOR_STATE_SENT ||
			cursor->status.dealloc == TDS_CURSOR_STATE_REQUESTED) {
			tdsdump_log(TDS_DBG_ERROR, "tds_cursor_dealloc(): freeing cursor \n");
		}
	}

	/* client will not use cursor anymore */
	tds_release_cursor(tds, cursor);

	return res;
}

static void
tds_quote_and_put(TDSSOCKET * tds, const char *s, const char *end)
{
	char buf[256];
	int i;

	CHECK_TDS_EXTRA(tds);
	
	for (i = 0; s != end; ++s) {
		buf[i++] = *s;
		if (*s == '\'')
			buf[i++] = '\'';
		if (i >= 254) {
			tds_put_string(tds, buf, i);
			i = 0;
		}
	}
	tds_put_string(tds, buf, i);
}

static int
tds_put_param_as_string(TDSSOCKET * tds, TDSPARAMINFO * params, int n)
{
	TDSCOLUMN *curcol = params->columns[n];
	CONV_RESULT cr;
	TDS_INT res;
	TDS_CHAR *src = (TDS_CHAR *) curcol->column_data;
	int src_len = curcol->column_cur_size;
	
	int i;
	char buf[256];
	int quote = 0;

	CHECK_TDS_EXTRA(tds);
	CHECK_PARAMINFO_EXTRA(params);

	if (src_len < 0) {
		/* on TDS 4 and 5 TEXT/IMAGE cannot be NULL, use empty */
		if (!IS_TDS7_PLUS(tds) && (curcol->column_type == SYBIMAGE || curcol->column_type == SYBTEXT))
			tds_put_string(tds, "''", 2);
		else
			tds_put_string(tds, "NULL", 4);
		return TDS_SUCCEED;
	}
	
	if (is_blob_type(curcol->column_type))
		src = ((TDSBLOB *)src)->textvalue;
	
	/* we could try to use only tds_convert but is not good in all cases */
	switch (curcol->column_type) {
	/* binary/char, do conversion in line */
	case SYBBINARY: case SYBVARBINARY: case SYBIMAGE: case XSYBBINARY: case XSYBVARBINARY:
		tds_put_n(tds, "0x", 2);
		for (i=0; src_len; ++src, --src_len) {
			buf[i++] = tds_hex_digits[*src >> 4 & 0xF];
			buf[i++] = tds_hex_digits[*src & 0xF];
			if (i == 256) {
				tds_put_string(tds, buf, i);
				i = 0;
			}
		}
		tds_put_string(tds, buf, i);
		break;
	/* char, quote as necessary */
	case SYBNVARCHAR: case SYBNTEXT: case XSYBNCHAR: case XSYBNVARCHAR:
		tds_put_string(tds, "N", 1);
	case SYBCHAR: case SYBVARCHAR: case SYBTEXT: case XSYBCHAR: case XSYBVARCHAR:
		tds_put_string(tds, "\'", 1);
		tds_quote_and_put(tds, src, src + src_len);
		tds_put_string(tds, "\'", 1);
		break;
	/* TODO date, use iso format */
	case SYBDATETIME:
	case SYBDATETIME4:
	case SYBDATETIMN:
		/* TODO use an ISO context */
	case SYBUNIQUE:
		quote = 1;
	default:
		res = tds_convert(tds->tds_ctx, tds_get_conversion_type(curcol->column_type, curcol->column_size), src, src_len, SYBCHAR, &cr);
		if (res < 0)
			return TDS_FAIL;
		if (quote)
			tds_put_string(tds, "\'", 1);
		tds_quote_and_put(tds, cr.c, cr.c + res);
		if (quote)
			tds_put_string(tds, "\'", 1);
		free(cr.c);
	}
	return TDS_SUCCEED;
}

/**
 * Emulate prepared execute traslating to a normal language
 */
static int
tds_send_emulated_execute(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params)
{
	int num_placeholders, i;
	const char *s, *e;

	CHECK_TDS_EXTRA(tds);

	assert(query);

	num_placeholders = tds_count_placeholders(query);
	if (num_placeholders && num_placeholders > params->num_cols)
		return TDS_FAIL;
	
	/* 
	 * NOTE: even for TDS5 we use this packet so to avoid computing 
	 * entire sql command
	 */
	tds->out_flag = TDS_QUERY;
	START_QUERY;
	if (!num_placeholders) {
		tds_put_string(tds, query, -1);
		return TDS_SUCCEED;
	}

	s = query;
	for (i = 0;; ++i) {
		e = tds_next_placeholder(s);
		tds_put_string(tds, s, e ? e - s : -1);
		if (!e)
			break;
		/* now translate parameter in string */
		tds_put_param_as_string(tds, params, i);

		s = e + 1;
	}
	
	return TDS_SUCCEED;
}

enum { MUL_STARTED = 1 };

int
tds_multiple_init(TDSSOCKET *tds, TDSMULTIPLE *multiple, TDS_MULTIPLE_TYPE type)
{
	multiple->type = type;
	multiple->flags = 0;

	if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
		return TDS_FAIL;

	tds->out_flag = TDS_QUERY;
	switch (type) {
	case TDS_MULTIPLE_QUERY:
		break;
	case TDS_MULTIPLE_EXECUTE:
	case TDS_MULTIPLE_RPC:
		if (IS_TDS7_PLUS(tds))
			tds->out_flag = TDS_RPC;
		break;
	}
	START_QUERY;

	return TDS_SUCCEED;
}

int
tds_multiple_done(TDSSOCKET *tds, TDSMULTIPLE *multiple)
{
	assert(tds && multiple);

	return tds_query_flush_packet(tds);
}

int
tds_multiple_query(TDSSOCKET *tds, TDSMULTIPLE *multiple, const char *query, TDSPARAMINFO * params)
{
	assert(multiple->type == TDS_MULTIPLE_QUERY);

	if (multiple->flags & MUL_STARTED)
		tds_put_string(tds, " ", 1);
	multiple->flags |= MUL_STARTED;

	return tds_send_emulated_execute(tds, query, params);
}

int
tds_multiple_execute(TDSSOCKET *tds, TDSMULTIPLE *multiple, TDSDYNAMIC * dyn)
{
	assert(multiple->type == TDS_MULTIPLE_EXECUTE);

	if (IS_TDS7_PLUS(tds)) {
		if (multiple->flags & MUL_STARTED) {
			/* TODO define constant */
			tds_put_byte(tds, IS_TDS90(tds) ? 0xff : 0x80);
		}
		multiple->flags |= MUL_STARTED;

		tds7_send_execute(tds, dyn);

		return TDS_SUCCEED;
	}

	if (multiple->flags & MUL_STARTED)
		tds_put_string(tds, " ", 1);
	multiple->flags |= MUL_STARTED;

	return tds_send_emulated_execute(tds, dyn->query, dyn->params);
}

int
tds_submit_optioncmd(TDSSOCKET * tds, TDS_OPTION_CMD command, TDS_OPTION option, TDS_OPTION_ARG *param, TDS_INT param_size)
{
	char cmd[128];
	char datefmt[4];
	TDS_INT resulttype;
	TDSCOLUMN *col;
	CONV_RESULT dres;
	int ctype;
	unsigned char*src;
	int srclen;
 
	CHECK_TDS_EXTRA(tds);
 
	tdsdump_log(TDS_DBG_FUNC, "tds_submit_optioncmd() \n");
 
	if (IS_TDS50(tds)) {
 
		if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING)
			return TDS_FAIL;
 
		tds->out_flag = TDS_NORMAL;
		tds_put_byte(tds, TDS_OPTIONCMD_TOKEN);
 
		tds_put_smallint(tds, 3 + param_size);
		tds_put_byte(tds, command);
		tds_put_byte(tds, option);
		tds_put_byte(tds, param_size);
		if (param_size)
			tds_put_n(tds, param, param_size);
 
		tds_query_flush_packet(tds);
 
		if (tds_process_simple_query(tds) == TDS_FAIL) {
			return TDS_FAIL;
		}
	}
 
	if (IS_TDS7_PLUS(tds)) {
		if (command == TDS_OPT_SET) {
			switch (option) {
			case TDS_OPT_ANSINULL :
				sprintf(cmd, "SET ANSI_NULLS %s", param->ti ? "ON" : "OFF");
				break;
			case TDS_OPT_ARITHABORTON :
				strcpy(cmd, "SET ARITHABORT ON");
				break;
			case TDS_OPT_ARITHABORTOFF :
				strcpy(cmd, "SET ARITHABORT OFF");
				break;
			case TDS_OPT_ARITHIGNOREON :
				strcpy(cmd, "SET ARITHIGNORE ON");
				break;
			case TDS_OPT_ARITHIGNOREOFF :
				strcpy(cmd, "SET ARITHIGNORE OFF");
				break;
			case TDS_OPT_CHAINXACTS :
				sprintf(cmd, "SET IMPLICIT_TRANSACTIONS %s", param->ti ? "ON" : "OFF");
				break;
			case TDS_OPT_CURCLOSEONXACT :
				sprintf(cmd, "SET CURSOR_CLOSE_ON_COMMIT %s", param->ti ? "ON" : "OFF");
				break;
			case TDS_OPT_NOCOUNT :
				sprintf(cmd, "SET NOCOUNT %s", param->ti ? "ON" : "OFF");
				break;
			case TDS_OPT_QUOTED_IDENT :
				sprintf(cmd, "SET QUOTED_IDENTIFIER %s", param->ti ? "ON" : "OFF");
				break;
			case TDS_OPT_TRUNCABORT :
				sprintf(cmd, "SET ANSI_WARNINGS %s", param->ti ? "OFF" : "ON");
				break;
			case TDS_OPT_DATEFIRST :
				sprintf(cmd, "SET DATEFIRST %d", param->ti);
				break;
			case TDS_OPT_DATEFORMAT :
				 switch (param->ti) {
					case TDS_OPT_FMTDMY: strcpy(datefmt,"dmy"); break;
					case TDS_OPT_FMTDYM: strcpy(datefmt,"dym"); break;
					case TDS_OPT_FMTMDY: strcpy(datefmt,"mdy"); break;
					case TDS_OPT_FMTMYD: strcpy(datefmt,"myd"); break;
					case TDS_OPT_FMTYDM: strcpy(datefmt,"ydm"); break;
					case TDS_OPT_FMTYMD: strcpy(datefmt,"ymd"); break;
				}
				sprintf(cmd, "SET DATEFORMAT %s", datefmt);
				break;
			case TDS_OPT_TEXTSIZE:
				sprintf(cmd, "SET TEXTSIZE %d", (int) param->i);
				break;
			/* TODO */
			case TDS_OPT_STAT_TIME:
			case TDS_OPT_STAT_IO:
			case TDS_OPT_ROWCOUNT:
			case TDS_OPT_NATLANG:
			case TDS_OPT_ISOLATION:
			case TDS_OPT_AUTHON:
			case TDS_OPT_CHARSET:
			case TDS_OPT_SHOWPLAN:
			case TDS_OPT_NOEXEC:
			case TDS_OPT_PARSEONLY:
			case TDS_OPT_GETDATA:
			case TDS_OPT_FORCEPLAN:
			case TDS_OPT_FORMATONLY:
			case TDS_OPT_FIPSFLAG:
			case TDS_OPT_RESTREES:
			case TDS_OPT_IDENTITYON:
			case TDS_OPT_CURREAD:
			case TDS_OPT_CURWRITE:
			case TDS_OPT_IDENTITYOFF:
			case TDS_OPT_AUTHOFF:
				break;
			}
			tds_submit_query(tds, cmd);
			if (tds_process_simple_query(tds) == TDS_FAIL) {
				return TDS_FAIL;
			}
		}
		if (command == TDS_OPT_LIST) {
			int optionval = 0;

			switch (option) {
			case TDS_OPT_ANSINULL :
			case TDS_OPT_ARITHABORTON :
			case TDS_OPT_ARITHABORTOFF :
			case TDS_OPT_ARITHIGNOREON :
			case TDS_OPT_ARITHIGNOREOFF :
			case TDS_OPT_CHAINXACTS :
			case TDS_OPT_CURCLOSEONXACT :
			case TDS_OPT_NOCOUNT :
			case TDS_OPT_QUOTED_IDENT :
			case TDS_OPT_TRUNCABORT :
				tdsdump_log(TDS_DBG_FUNC, "SELECT @@options\n");
				strcpy(cmd, "SELECT @@options");
				break;
			case TDS_OPT_DATEFIRST :
				strcpy(cmd, "SELECT @@datefirst");
				break;
			case TDS_OPT_DATEFORMAT :
				strcpy(cmd, "SELECT DATEPART(dy,'01/02/03')");
				break;
			case TDS_OPT_TEXTSIZE:
				strcpy(cmd, "SELECT @@textsize");
				break;
			/* TODO */
			case TDS_OPT_STAT_TIME:
			case TDS_OPT_STAT_IO:
			case TDS_OPT_ROWCOUNT:
			case TDS_OPT_NATLANG:
			case TDS_OPT_ISOLATION:
			case TDS_OPT_AUTHON:
			case TDS_OPT_CHARSET:
			case TDS_OPT_SHOWPLAN:
			case TDS_OPT_NOEXEC:
			case TDS_OPT_PARSEONLY:
			case TDS_OPT_GETDATA:
			case TDS_OPT_FORCEPLAN:
			case TDS_OPT_FORMATONLY:
			case TDS_OPT_FIPSFLAG:
			case TDS_OPT_RESTREES:
			case TDS_OPT_IDENTITYON:
			case TDS_OPT_CURREAD:
			case TDS_OPT_CURWRITE:
			case TDS_OPT_IDENTITYOFF:
			case TDS_OPT_AUTHOFF:
			default:
				tdsdump_log(TDS_DBG_FUNC, "what!\n");
			}
			tds_submit_query(tds, cmd);
			while (tds_process_tokens(tds, &resulttype, NULL, TDS_TOKEN_RESULTS) == TDS_SUCCEED) {
				switch (resulttype) {
				case TDS_ROWFMT_RESULT:
					break;
				case TDS_ROW_RESULT:
					while (tds_process_tokens(tds, &resulttype, NULL, TDS_STOPAT_ROWFMT|TDS_RETURN_DONE|TDS_RETURN_ROW) == TDS_SUCCEED) {
						if (resulttype != TDS_ROW_RESULT)
							break;
 
						if (!tds->current_results)
							continue;
 
						col = tds->current_results->columns[0];
						ctype = tds_get_conversion_type(col->column_type, col->column_size);
 
						src = col->column_data;
						srclen = col->column_cur_size;
 
 
						tds_convert(tds->tds_ctx, ctype, (TDS_CHAR *) src, srclen, SYBINT4, &dres);
						optionval = dres.i;
					}
					break;
				default:
					break;
				}
			}
			tdsdump_log(TDS_DBG_FUNC, "optionval = %d\n", optionval);
			switch (option) {
			case TDS_OPT_CHAINXACTS :
				tds->option_value = (optionval & 0x02) > 0;
				break;
			case TDS_OPT_CURCLOSEONXACT :
				tds->option_value = (optionval & 0x04) > 0;
				break;
			case TDS_OPT_TRUNCABORT :
				tds->option_value = (optionval & 0x08) > 0;
				break;
			case TDS_OPT_ANSINULL :
				tds->option_value = (optionval & 0x20) > 0;
				break;
			case TDS_OPT_ARITHABORTON :
				tds->option_value = (optionval & 0x40) > 0;
				break;
			case TDS_OPT_ARITHABORTOFF :
				tds->option_value = (optionval & 0x40) > 0;
				break;
			case TDS_OPT_ARITHIGNOREON :
				tds->option_value = (optionval & 0x80) > 0;
				break;
			case TDS_OPT_ARITHIGNOREOFF :
				tds->option_value = (optionval & 0x80) > 0;
				break;
			case TDS_OPT_QUOTED_IDENT :
				tds->option_value = (optionval & 0x0100) > 0;
				break;
			case TDS_OPT_NOCOUNT :
				tds->option_value = (optionval & 0x0200) > 0;
				break;
			case TDS_OPT_TEXTSIZE:
			case TDS_OPT_DATEFIRST :
				tds->option_value = optionval;
				break;
			case TDS_OPT_DATEFORMAT :
				switch (optionval) {
				case 61: tds->option_value = TDS_OPT_FMTYDM; break;
				case 34: tds->option_value = TDS_OPT_FMTYMD; break;
				case 32: tds->option_value = TDS_OPT_FMTDMY; break;
				case 60: tds->option_value = TDS_OPT_FMTYDM; break;
				case 2:  tds->option_value = TDS_OPT_FMTMDY; break;
				case 3:  tds->option_value = TDS_OPT_FMTMYD; break;
				}
				break;
			/* TODO */
			case TDS_OPT_STAT_TIME:
			case TDS_OPT_STAT_IO:
			case TDS_OPT_ROWCOUNT:
			case TDS_OPT_NATLANG:
			case TDS_OPT_ISOLATION:
			case TDS_OPT_AUTHON:
			case TDS_OPT_CHARSET:
			case TDS_OPT_SHOWPLAN:
			case TDS_OPT_NOEXEC:
			case TDS_OPT_PARSEONLY:
			case TDS_OPT_GETDATA:
			case TDS_OPT_FORCEPLAN:
			case TDS_OPT_FORMATONLY:
			case TDS_OPT_FIPSFLAG:
			case TDS_OPT_RESTREES:
			case TDS_OPT_IDENTITYON:
			case TDS_OPT_CURREAD:
			case TDS_OPT_CURWRITE:
			case TDS_OPT_IDENTITYOFF:
			case TDS_OPT_AUTHOFF:
				break;
			}
			tdsdump_log(TDS_DBG_FUNC, "tds_submit_optioncmd: returned option_value = %d\n", tds->option_value);
		}
	}
	return TDS_SUCCEED;
}

/*
 * TODO add function to return type suitable for param
 * ie:
 * sybvarchar -> sybvarchar / xsybvarchar
 * sybint4 -> sybintn
 */

/** @} */
