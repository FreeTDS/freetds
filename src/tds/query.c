/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011  Frediano Ziglio
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

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <ctype.h>

#include <freetds/tds.h>
#include <freetds/enum_cap.h>
#include <freetds/iconv.h>
#include <freetds/convert.h>
#include <freetds/utils/string.h>
#include <freetds/checks.h>
#include <freetds/stream.h>
#include <freetds/bytes.h>
#include <freetds/replacements.h>

#include <assert.h>

static TDSRET tds5_put_params(TDSSOCKET * tds, TDSPARAMINFO * info, int flags) TDS_WUR;
static void tds7_put_query_params(TDSSOCKET * tds, const char *query, size_t query_len);
static TDSRET tds_put_data_info(TDSSOCKET * tds, TDSCOLUMN * curcol, int flags);
static inline TDSRET tds_put_data(TDSSOCKET * tds, TDSCOLUMN * curcol);
static TDSRET tds7_write_param_def_from_query(TDSSOCKET * tds, const char* converted_query,
					      size_t converted_query_len, TDSPARAMINFO * params) TDS_WUR;
static TDSRET tds7_write_param_def_from_params(TDSSOCKET * tds, const char* query, size_t query_len,
					       TDSPARAMINFO * params) TDS_WUR;

static TDSRET tds_put_param_as_string(TDSSOCKET * tds, TDSPARAMINFO * params, int n);
static TDSRET tds_send_emulated_execute(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params);
static int tds_count_placeholders_ucs2le(const char *query, const char *query_end);

#define TDS_PUT_DATA_USE_NAME 1
#define TDS_PUT_DATA_PREFIX_NAME 2
#define TDS_PUT_DATA_LONG_STATUS 4

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
 * The input is NUL-terminated, but the output does not contains the NUL.
 * \param buffer buffer where to store output
 * \param buf string to write
 * \return bytes written
 */
static size_t
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

/**
 * Utility to convert a constant ascii string to ucs2 and send to server.
 * Used to send internal store procedure names to server.
 * \tds
 * \param s  constanst string to send
 */
#define TDS_PUT_N_AS_UCS2(tds, s) do { \
	char buffer[sizeof(s)*2-2]; \
	tds_put_smallint(tds, sizeof(buffer)/2); \
	tds_put_n(tds, buffer, tds_ascii_to_ucs2(buffer, s)); \
} while(0)

/**
 * Convert a string in an allocated buffer
 * \param tds        state information for the socket and the TDS protocol
 * \param char_conv  information about the encodings involved
 * \param s          input string
 * \param len        input string length (in bytes), -1 for NUL-terminated
 * \param out_len    returned output length (in bytes)
 * \return string allocated (or input pointer if no conversion required) or NULL if error
 */
const char *
tds_convert_string(TDSSOCKET * tds, TDSICONV * char_conv, const char *s, int len, size_t *out_len)
{
	char *buf;

	const char *ib;
	char *ob;
	size_t il, ol;

	/* char_conv is only mostly const */
	TDS_ERRNO_MESSAGE_FLAGS *suppress = (TDS_ERRNO_MESSAGE_FLAGS*) &char_conv->suppress;

	CHECK_TDS_EXTRA(tds);

	il = len < 0 ? strlen(s) : (size_t) len;
	if (char_conv->flags == TDS_ENCODING_MEMCPY) {
		*out_len = il;
		return s;
	}

	/* allocate needed buffer (+1 is to exclude 0 case) */
	ol = il * char_conv->to.charset.max_bytes_per_char / char_conv->from.charset.min_bytes_per_char + 1;
	buf = tds_new(char, ol);
	if (!buf) {
		*out_len = 0;
		return NULL;
	}

	ib = s;
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
void
tds_convert_string_free(const char *original, const char *converted)
{
	if (original != converted)
		free((char *) converted);
}
#endif

/**
 * Flush query packet.
 * Used at the end of packet write to really send packet to server.
 * This also changes the state to TDS_PENDING.
 * \tds
 */
static TDSRET
tds_query_flush_packet(TDSSOCKET *tds)
{
	TDSRET ret = tds_flush_packet(tds);
	/* TODO depend on result ?? */
	tds_set_state(tds, TDS_PENDING);
	return ret;
}

/**
 * Set current dynamic.
 * \tds
 * \param dyn  dynamic to set
 */
void
tds_set_cur_dyn(TDSSOCKET *tds, TDSDYNAMIC *dyn)
{
	if (dyn)
		++dyn->ref_count;
	tds_release_cur_dyn(tds);
	tds->cur_dyn = dyn;
}

/**
 * Sends a language string to the database server for
 * processing.  TDS 4.2 is a plain text message with a packet type of 0x01,
 * TDS 7.0 is a unicode string with packet type 0x01, and TDS 5.0 uses a 
 * TDS_LANGUAGE_TOKEN to encapsulate the query and a packet type of 0x0f.
 * \tds
 * \param query language query to submit
 * \return TDS_FAIL or TDS_SUCCESS
 */
TDSRET
tds_submit_query(TDSSOCKET * tds, const char *query)
{
	return tds_submit_query_params(tds, query, NULL, NULL);
}

/**
 * Substitute ?-style placeholders with named (\@param) ones.
 * Sybase does not support ?-style placeholders so convert them.
 * Also the function replace parameter names.
 * \param query  query string
 * \param[in,out] query_len  pointer to query length.
 *                On input length of input query, on output length
 *                of output query
 * \param params  parameters to send to server
 * \returns new query or NULL on error
 */
static char *
tds5_fix_dot_query(const char *query, size_t *query_len, TDSPARAMINFO * params)
{
	int i;
	size_t len, pos;
	const char *e, *s;
	size_t size = *query_len + 30;
	char colname[32];
	char *out;

	out = tds_new(char, size);
	if (!out)
		goto memory_error;
	pos = 0;

	s = query;
	for (i = 0;; ++i) {
		e = tds_next_placeholder(s);
		len = e ? e - s : strlen(s);
		if (pos + len + 12 >= size) {
			size = pos + len + 30;
			if (!TDS_RESIZE(out, size))
				goto memory_error;
		}
		memcpy(out + pos, s, len);
		pos += len;
		if (!e)
			break;
		pos += sprintf(out + pos, "@P%d", i + 1);
		if (!params || i >= params->num_cols)
			goto memory_error;
		sprintf(colname, "@P%d", i + 1);
		if (!tds_dstr_copy(&params->columns[i]->column_name, colname))
			goto memory_error;

		s = e + 1;
	}
	out[pos] = 0;
	*query_len = pos;
	return out;

memory_error:
	free(out);
	return NULL;
}

/**
 * Write data to wire
 * \tds
 * \param curcol  column where store column information
 * \return TDS_FAIL on error or TDS_SUCCESS
 */
static inline TDSRET
tds_put_data(TDSSOCKET * tds, TDSCOLUMN * curcol)
{
	return curcol->funcs->put_data(tds, curcol, 0);
}

/**
 * Start query packet of a given type
 * \tds
 * \param packet_type  packet type
 * \param head         extra information to put in a TDS7 header
 */
static TDSRET
tds_start_query_head(TDSSOCKET *tds, unsigned char packet_type, TDSHEADERS * head)
{
	tds->out_flag = packet_type;
	if (IS_TDS72_PLUS(tds->conn)) {
		TDSFREEZE outer;

		tds_freeze(tds, &outer, 4);                    /* total length */
		tds_put_int(tds, 18);                          /* length: transaction descriptor */
		tds_put_smallint(tds, 2);                      /* type: transaction descriptor */
		tds_put_n(tds, tds->conn->tds72_transaction, 8);  /* transaction */
		tds_put_int(tds, 1);                           /* request count */
		if (head && head->qn_msgtext && head->qn_options) {
			TDSFREEZE query;

			tds_freeze(tds, &query, 4);                    /* length: query notification */
			tds_put_smallint(tds, 1);                      /* type: query notification */

			TDS_START_LEN_USMALLINT(tds) {
				tds_put_string(tds, head->qn_msgtext, -1);     /* notifyid */
			} TDS_END_LEN

			TDS_START_LEN_USMALLINT(tds) {
				tds_put_string(tds, head->qn_options, -1);     /* ssbdeployment */
			} TDS_END_LEN

			if (head->qn_timeout != 0)
				tds_put_int(tds, head->qn_timeout);    /* timeout */

			tds_freeze_close_len(&query, tds_freeze_written(&query));
		}
		tds_freeze_close_len(&outer, tds_freeze_written(&outer));
	}
	return TDS_SUCCESS;
}

/**
 * Start query packet of a given type
 * \tds
 * \param packet_type  packet type
 */
void
tds_start_query(TDSSOCKET *tds, unsigned char packet_type)
{
	/* no need to check return value here because tds_start_query_head() cannot
	fail when given a NULL head parameter */
	tds_start_query_head(tds, packet_type, NULL);
}

/**
 * Sends a language string to the database server for
 * processing.  TDS 4.2 is a plain text message with a packet type of 0x01,
 * TDS 7.0 is a unicode string with packet type 0x01, and TDS 5.0 uses a
 * TDS_LANGUAGE_TOKEN to encapsulate the query and a packet type of 0x0f.
 * \tds
 * \param query  language query to submit
 * \param params parameters of query
 * \return TDS_FAIL or TDS_SUCCESS
 */
TDSRET
tds_submit_query_params(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params, TDSHEADERS * head)
{
	size_t query_len;
	int num_params = params ? params->num_cols : 0;
 
	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);
 
	if (!query)
		return TDS_FAIL;
 
	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;
 
	query_len = strlen(query);
 
	if (IS_TDS50(tds->conn)) {
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
		TDS_START_LEN_UINT(tds) {
			tds_put_byte(tds, params ? 1 : 0);  /* 1 if there are params, 0 otherwise */
			tds_put_string(tds, query, query_len);
		} TDS_END_LEN
		if (params) {
			/* add on parameters */
			int flags = tds_dstr_isempty(&params->columns[0]->column_name) ? 0 : TDS_PUT_DATA_USE_NAME;
			TDS_PROPAGATE(tds5_put_params(tds, params, flags));
		}
		free(new_query);
	} else if (!IS_TDS7_PLUS(tds->conn) || !params || !params->num_cols) {
		if (tds_start_query_head(tds, TDS_QUERY, head) != TDS_SUCCESS)
			return TDS_FAIL;
		tds_put_string(tds, query, (int)query_len);
	} else {
		TDSCOLUMN *param;
		int count, i;
		size_t converted_query_len;
		const char *converted_query;
		TDSFREEZE outer;
		TDSRET rc;

		converted_query = tds_convert_string(tds, tds->conn->char_convs[client2ucs2], query, (int)query_len, &converted_query_len);
		if (!converted_query) {
			tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}

		count = tds_count_placeholders_ucs2le(converted_query, converted_query + converted_query_len);

		if (tds_start_query_head(tds, TDS_RPC, head) != TDS_SUCCESS) {
			tds_convert_string_free(query, converted_query);
			return TDS_FAIL;
		}

		tds_freeze(tds, &outer, 0);

		/* procedure name */
		if (IS_TDS71_PLUS(tds->conn)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_EXECUTESQL);
		} else {
			TDS_PUT_N_AS_UCS2(tds, "sp_executesql");
		}
		tds_put_smallint(tds, 0);
 
		/* string with sql statement */
		if (!count) {
			tds_put_byte(tds, 0);
			tds_put_byte(tds, 0);
			tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
			TDS_PUT_INT(tds, converted_query_len);
			if (IS_TDS71_PLUS(tds->conn))
				tds_put_n(tds, tds->conn->collation, 5);
			TDS_PUT_INT(tds, converted_query_len);
			tds_put_n(tds, converted_query, converted_query_len);

			rc = tds7_write_param_def_from_params(tds, converted_query, converted_query_len, params);
		} else {
			tds7_put_query_params(tds, converted_query, converted_query_len);

			rc = tds7_write_param_def_from_query(tds, converted_query, converted_query_len, params);
		}
		tds_convert_string_free(query, converted_query);
		if (TDS_FAILED(rc)) {
			tds_freeze_abort(&outer);
			return rc;
		}
		tds_freeze_close(&outer);

		for (i = 0; i < num_params; i++) {
			param = params->columns[i];
			TDS_PROPAGATE(tds_put_data_info(tds, param, 0));
			TDS_PROPAGATE(tds_put_data(tds, param));
		}
		tds->current_op = TDS_OP_EXECUTESQL;
	}
	return tds_query_flush_packet(tds);
}

/**
 * Format and submit a query
 * \tds
 * \param queryf  query format. printf like expansion is performed on
 *        this query.
 */
TDSRET
tds_submit_queryf(TDSSOCKET * tds, const char *queryf, ...)
{
	va_list ap;
	char *query = NULL;
	TDSRET rc = TDS_FAIL;

	CHECK_TDS_EXTRA(tds);

	va_start(ap, queryf);
	if (vasprintf(&query, queryf, ap) >= 0) {
		rc = tds_submit_query(tds, query);
		free(query);
	}
	va_end(ap);
	return rc;
}

/**
 * Skip a comment in a query
 * \param s    start of the string (or part of it)
 * \returns pointer to end of comment
 */
const char *
tds_skip_comment(const char *s)
{
	const char *p = s;

	if (*p == '-' && p[1] == '-') {
		for (;*++p != '\0';)
			if (*p == '\n')
				return p + 1;
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
 * \param s pointer to first quoting character. @verbatim Should be ', " or [. @endverbatim
 * \return character after quoting
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
 * \param start pointer to part of query to search
 * \return next placeholder or NULL if not found
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
 * Count the number of placeholders ('?') in a query
 * \param query  query string
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

/**
 * Skip a comment in a query
 * \param s    start of the string (or part of it). Encoded in ucs2le
 * \param end  end of string
 * \returns pointer to end of comment
 */
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
		return end + 2;
	} else
		p += 2;

	return p;
}


/**
 * Return pointer to end of a quoted string.
 * At the beginning pointer should point to delimiter.
 * \param s    start of string to skip encoded in ucs2le
 * \param end  pointer to end of string
 */
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

/**
 * Found the next placeholder (? or \@param) in a string.
 * String must be encoded in ucs2le.
 * \param start  start of the string (or part of it)
 * \param end    end of string
 * \param named  true if named parameters should be returned
 * \returns either start of next placeholder or end if not found
 */
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

/**
 * Count the number of placeholders ('?') in a query
 * \param query      query encoded in ucs2le
 * \param query_end  end of query
 * \return number of placeholders found
 */
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

static const char*
tds50_char_declaration_from_usertype(TDSSOCKET *tds, TDS_INT usertype, unsigned int *p_size)
{
	switch (usertype) {
	case USER_CHAR_TYPE:
		return "CHAR(%u)";
	case USER_VARCHAR_TYPE:
		return "VARCHAR(%u)";
	case USER_SYSNAME_TYPE:
		return "SYSNAME";
	case USER_NCHAR_TYPE:
		*p_size /= tds->conn->ncharsize;
		return "NCHAR(%u)";
	case USER_NVARCHAR_TYPE:
		*p_size /= tds->conn->ncharsize;
		return "NVARCHAR(%u)";
	}
	return NULL;
}

/**
 * Return declaration for column (like "varchar(20)").
 *
 * This depends on:
 * - on_server.column_type
 * - varint_size (for varchar(max) distinction)
 * - column_size
 * - precision/scale (numeric)
 *
 * \tds
 * \param curcol column
 * \param out    buffer to hold declaration
 * \return TDS_FAIL or TDS_SUCCESS
 */
TDSRET
tds_get_column_declaration(TDSSOCKET * tds, TDSCOLUMN * curcol, char *out)
{
	const char *fmt = NULL;
	/* unsigned int is required by printf format, don't use size_t */
	unsigned int max_len = IS_TDS7_PLUS(tds->conn) ? 8000 : 255;
	unsigned int size;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(curcol);

	size = tds_fix_column_size(tds, curcol);

	switch (tds_get_conversion_type(curcol->on_server.column_type, curcol->on_server.column_size)) {
	case XSYBCHAR:
		if (IS_TDS50(tds->conn)) {
			max_len = 32767;
			fmt = tds50_char_declaration_from_usertype(tds, curcol->column_usertype, &size);
			if (fmt != NULL)
				break;
		}
	case SYBCHAR:
		fmt = "CHAR(%u)";
		break;
	case SYBVARCHAR:
		if (IS_TDS50(tds->conn)) {
			fmt = tds50_char_declaration_from_usertype(tds, curcol->column_usertype, &size);
			if (fmt != NULL)
				break;
		}
	case XSYBVARCHAR:
		if (curcol->column_varint_size == 8)
			fmt = "VARCHAR(MAX)";
		else
			fmt = "VARCHAR(%u)";
		break;
	case SYBUINT1:
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
	case SYBDATE:
		fmt = "DATE";
		break;
	case SYBTIME:
		fmt = "TIME";
		break;
	case SYBBIT:
		fmt = "BIT";
		break;
	case SYBTEXT:
		fmt = "TEXT";
		break;
	case SYBLONGBINARY:	/* TODO correct ?? */
	case SYBIMAGE:
		if (IS_TDS50(tds->conn)) {
			switch (curcol->column_usertype) {
			case USER_UNICHAR_TYPE:
				size /= 2u;
				max_len = 8192;
				fmt = "UNICHAR(%u)";
				break;
			case USER_UNIVARCHAR_TYPE:
				size /= 2u;
				max_len = 8192;
				fmt = "UNIVARCHAR(%u)";
				break;
			case USER_UNITEXT_TYPE:
				fmt = "UNITEXT";
				break;
			}
			if (fmt != NULL)
				break;
		}
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
		fmt = "BINARY(%u)";
		break;
	case SYBVARBINARY:
	case XSYBVARBINARY:
		if (curcol->column_varint_size == 8)
			fmt = "VARBINARY(MAX)";
		else
			fmt = "VARBINARY(%u)";
		break;
	case SYBNUMERIC:
		fmt = "NUMERIC(%d,%d)";
		goto numeric_decimal;
	case SYBDECIMAL:
		fmt = "DECIMAL(%d,%d)";
	      numeric_decimal:
		sprintf(out, fmt, curcol->column_prec, curcol->column_scale);
		return TDS_SUCCESS;
		break;
	case SYBUNIQUE:
		if (IS_TDS7_PLUS(tds->conn))
			fmt = "UNIQUEIDENTIFIER";
		break;
	case SYBNTEXT:
		if (IS_TDS7_PLUS(tds->conn))
			fmt = "NTEXT";
		break;
	case SYBNVARCHAR:
	case XSYBNVARCHAR:
		if (curcol->column_varint_size == 8) {
			fmt = "NVARCHAR(MAX)";
		} else if (IS_TDS7_PLUS(tds->conn)) {
			fmt = "NVARCHAR(%u)";
			max_len = 4000;
			size /= 2u;
		}
		break;
	case XSYBNCHAR:
		if (IS_TDS7_PLUS(tds->conn)) {
			fmt = "NCHAR(%u)";
			max_len = 4000;
			size /= 2u;
		}
		break;
	case SYBVARIANT:
		if (IS_TDS7_PLUS(tds->conn))
			fmt = "SQL_VARIANT";
		break;
		/* TODO support scale !! */
	case SYBMSTIME:
		fmt = "TIME";
		break;
	case SYBMSDATE:
		fmt = "DATE";
		break;
	case SYBMSDATETIME2:
		fmt = "DATETIME2";
		break;
	case SYBMSDATETIMEOFFSET:
		fmt = "DATETIMEOFFSET";
		break;
	case SYB5BIGTIME:
		fmt = "BIGTIME";
		break;
	case SYB5BIGDATETIME:
		fmt = "BIGDATETIME";
		break;
	case SYBUINT2:
		fmt = "UNSIGNED SMALLINT";
		break;
	case SYBUINT4:
		fmt = "UNSIGNED INT";
		break;
	case SYBUINT8:
		fmt = "UNSIGNED BIGINT";
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
	default:
		tdsdump_log(TDS_DBG_ERROR, "Unknown type %d\n", tds_get_conversion_type(curcol->on_server.column_type, curcol->on_server.column_size));
		break;
	}

	if (fmt) {
		/* fill out */
		sprintf(out, fmt, size > 0 ? MIN(size, max_len) : 1u);
		return TDS_SUCCESS;
	}

	out[0] = 0;
	return TDS_FAIL;
}

/**
 * Write string with parameters definition, useful for TDS7+.
 * Looks like "@P1 INT, @P2 VARCHAR(100)"
 * \param tds     state information for the socket and the TDS protocol
 * \param converted_query     query to send to server in ucs2le encoding
 * \param converted_query_len query length in bytes
 * \param params  parameters to build declaration
 * \return result of write
 */
/* TODO find a better name for this function */
static TDSRET
tds7_write_param_def_from_query(TDSSOCKET * tds, const char* converted_query, size_t converted_query_len, TDSPARAMINFO * params)
{
	char declaration[128], *p;
	int i, count;
	size_t written;
	TDSFREEZE outer, inner;

	assert(IS_TDS7_PLUS(tds->conn));

	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);

	count = tds_count_placeholders_ucs2le(converted_query, converted_query + converted_query_len);

	/* string with parameters types */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */

	/* put parameters definitions */
	tds_freeze(tds, &outer, 4);
	if (IS_TDS71_PLUS(tds->conn))
		tds_put_n(tds, tds->conn->collation, 5);
	tds_freeze(tds, &inner, 4);

	for (i = 0; i < count; ++i) {
		p = declaration;
		if (i)
			*p++ = ',';

		/* get this parameter declaration */
		p += sprintf(p, "@P%d ", i+1);
		if (!params || i >= params->num_cols) {
			strcpy(p, "varchar(4000)");
		} else if (TDS_FAILED(tds_get_column_declaration(tds, params->columns[i], p))) {
			tds_freeze_abort(&inner);
			tds_freeze_abort(&outer);
			return TDS_FAIL;
		}

		tds_put_string(tds, declaration, -1);
	}

	written = tds_freeze_written(&inner) - 4;
	tds_freeze_close_len(&inner, written ? written : -1);
	tds_freeze_close_len(&outer, written);
	return TDS_SUCCESS;
}

/**
 * Write string with parameters definition, useful for TDS7+.
 * Looks like "@P1 INT, @P2 VARCHAR(100)"
 * \param tds       state information for the socket and the TDS protocol
 * \param query     query to send to server encoded in ucs2le
 * \param query_len query length in bytes
 * \param params    parameters to build declaration
 * \return result of the operation
 */
/* TODO find a better name for this function */
static TDSRET
tds7_write_param_def_from_params(TDSSOCKET * tds, const char* query, size_t query_len, TDSPARAMINFO * params)
{
	char declaration[40];
	int i;
	struct tds_ids {
		const char *p;
		size_t len;
	} *ids = NULL;
	TDSFREEZE outer, inner;
	size_t written;

	assert(IS_TDS7_PLUS(tds->conn));

	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);

	/* string with parameters types */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 0);
	tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */

	/* put parameters definitions */
	tds_freeze(tds, &outer, 4);
	if (IS_TDS71_PLUS(tds->conn))
		tds_put_n(tds, tds->conn->collation, 5);
	tds_freeze(tds, &inner, 4);

	if (!params || !params->num_cols) {
		tds_freeze_close_len(&inner, -1);
		tds_freeze_close_len(&outer, 0);
		return TDS_SUCCESS;
	}

	/* try to detect missing names */
	ids = tds_new0(struct tds_ids, params->num_cols);
	if (!ids)
		goto Cleanup;
	if (tds_dstr_isempty(&params->columns[0]->column_name)) {
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

	for (i = 0; i < params->num_cols; ++i) {
		if (i)
			tds_put_smallint(tds, ',');

		/* this part of buffer can be not-ascii compatible, use all ucs2... */
		if (ids[i].p) {
			tds_put_n(tds, ids[i].p, ids[i].len);
		} else {
			tds_put_string(tds, tds_dstr_cstr(&params->columns[i]->column_name),
				       tds_dstr_len(&params->columns[i]->column_name));
		}
		tds_put_smallint(tds, ' ');

		/* get this parameter declaration */
		tds_get_column_declaration(tds, params->columns[i], declaration);
		if (!declaration[0])
			goto Cleanup;
		tds_put_string(tds, declaration, -1);
	}
	free(ids);

	written = tds_freeze_written(&inner) - 4;
	tds_freeze_close_len(&inner, written);
	tds_freeze_close_len(&outer, written);

	return TDS_SUCCESS;

      Cleanup:
	free(ids);
	tds_freeze_abort(&inner);
	tds_freeze_abort(&outer);
	return TDS_FAIL;
}


/**
 * Output params types and query (required by sp_prepare/sp_executesql/sp_prepexec)
 * \param tds       state information for the socket and the TDS protocol
 * \param query     query (encoded in ucs2le)
 * \param query_len query length in bytes
 */
static void
tds7_put_query_params(TDSSOCKET * tds, const char *query, size_t query_len)
{
	size_t len;
	int i, num_placeholders;
	const char *s, *e;
	char buf[24];
	const char *const query_end = query + query_len;

	CHECK_TDS_EXTRA(tds);

	assert(IS_TDS7_PLUS(tds->conn));

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
	len = 2u * len + query_len;
	TDS_PUT_INT(tds, len);
	if (IS_TDS71_PLUS(tds->conn))
		tds_put_n(tds, tds->conn->collation, 5);
	TDS_PUT_INT(tds, len);
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

/**
 * Creates a temporary stored procedure in the server.
 *
 * Under TDS 4.2 dynamic statements are emulated building sql command.
 * TDS 5 does not uses parameters type.
 * TDS 7+ uses parameter types to prepare the query. You should
 * prepare again the query if parameters changes.
 * \param tds     state information for the socket and the TDS protocol
 * \param query   language query with given placeholders (?)
 * \param id      string to identify the dynamic query. Pass NULL for automatic generation.
 * \param dyn_out will receive allocated TDSDYNAMIC*. Any older allocated dynamic won't be freed, Can be NULL.
 * \param params  parameters to use. It can be NULL even if parameters are present. Used only for TDS7+
 * \return TDS_FAIL or TDS_SUCCESS
 */
/* TODO parse all results ?? */
TDSRET
tds_submit_prepare(TDSSOCKET * tds, const char *query, const char *id, TDSDYNAMIC ** dyn_out, TDSPARAMINFO * params)
{
	int query_len;
	TDSRET rc = TDS_FAIL;
	TDSDYNAMIC *dyn;

	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);

	if (!query || !dyn_out)
		return TDS_FAIL;

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	/* allocate a structure for this thing */
	dyn = tds_alloc_dynamic(tds->conn, id);
	if (!dyn)
		return TDS_FAIL;
	tds_release_dynamic(dyn_out);
	*dyn_out = dyn;
	tds_release_cur_dyn(tds);

	/* TDS5 sometimes cannot accept prepare so we need to store query */
	if (!IS_TDS7_PLUS(tds->conn)) {
		dyn->query = strdup(query);
		if (!dyn->query)
			goto failure;
	}

	if (!IS_TDS50(tds->conn) && !IS_TDS7_PLUS(tds->conn)) {
		dyn->emulated = 1;
		tds_dynamic_deallocated(tds->conn, dyn);
		tds_set_state(tds, TDS_IDLE);
		return TDS_SUCCESS;
	}

	query_len = (int)strlen(query);

	tds_set_cur_dyn(tds, dyn);

	if (IS_TDS7_PLUS(tds->conn)) {
		size_t converted_query_len;
		const char *converted_query;
		TDSFREEZE outer;
		TDSRET rc;

		converted_query = tds_convert_string(tds, tds->conn->char_convs[client2ucs2], query, query_len, &converted_query_len);
		if (!converted_query)
			goto failure;

		tds_freeze(tds, &outer, 0);
		tds_start_query(tds, TDS_RPC);
		/* procedure name */
		if (IS_TDS71_PLUS(tds->conn)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_PREPARE);
		} else {
			TDS_PUT_N_AS_UCS2(tds, "sp_prepare");
		}
		tds_put_smallint(tds, 0);

		/* return param handle (int) */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 1);	/* result */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 0);

		rc = tds7_write_param_def_from_query(tds, converted_query, converted_query_len, params);
		tds7_put_query_params(tds, converted_query, converted_query_len);
		tds_convert_string_free(query, converted_query);
		if (TDS_FAILED(rc)) {
			tds_freeze_abort(&outer);
			return rc;
		}
		tds_freeze_close(&outer);

		/* options, 1 == RETURN_METADATA */
		tds_put_byte(tds, 0);
		tds_put_byte(tds, 0);
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, 1);

		tds->current_op = TDS_OP_PREPARE;
	} else {
		tds->out_flag = TDS_NORMAL;

		tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
		TDS_START_LEN_USMALLINT(tds) {
			tds_put_byte(tds, TDS_DYN_PREPARE);
			tds_put_byte(tds, 0x00);
			TDS_START_LEN_TINYINT(tds) {
				tds_put_string(tds, dyn->id, -1);
			} TDS_END_LEN

			/* TODO how to pass parameters type? like store procedures ? */
			TDS_START_LEN_USMALLINT(tds) {
				if (tds_capability_has_req(tds->conn, TDS_REQ_PROTO_DYNPROC)) {
					tds_put_n(tds, "create proc ", 12);
					tds_put_string(tds, dyn->id, -1);
					tds_put_n(tds, " as ", 4);
				}
				tds_put_string(tds, query, query_len);
			} TDS_END_LEN
		} TDS_END_LEN
	}

	rc = tds_query_flush_packet(tds);
	if (TDS_SUCCEED(rc))
		return rc;

failure:
	/* TODO correct if writing fail ?? */
	tds_set_state(tds, TDS_IDLE);

	tds_release_dynamic(dyn_out);
	tds_dynamic_deallocated(tds->conn, dyn);
	return rc;
}

/**
 * Submit a prepared query with parameters
 * \param tds     state information for the socket and the TDS protocol
 * \param query   language query with given placeholders (?)
 * \param params  parameters to send
 * \return TDS_FAIL or TDS_SUCCESS
 */
TDSRET
tds_submit_execdirect(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params, TDSHEADERS * head)
{
	size_t query_len;
	TDSCOLUMN *param;
	TDSDYNAMIC *dyn;
	size_t id_len;
	TDSFREEZE outer;

	CHECK_TDS_EXTRA(tds);
	CHECK_PARAMINFO_EXTRA(params);

	if (!query)
		return TDS_FAIL;
	query_len = strlen(query);

	if (IS_TDS7_PLUS(tds->conn)) {
		int i;
		size_t converted_query_len;
		const char *converted_query;
		TDSRET rc;

		if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
			return TDS_FAIL;

		converted_query = tds_convert_string(tds, tds->conn->char_convs[client2ucs2], query, (int)query_len, &converted_query_len);
		if (!converted_query) {
			tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}

		if (tds_start_query_head(tds, TDS_RPC, head) != TDS_SUCCESS) {
			tds_convert_string_free(query, converted_query);
			return TDS_FAIL;
		}
		tds_freeze(tds, &outer, 0);
		/* procedure name */
		if (IS_TDS71_PLUS(tds->conn)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_EXECUTESQL);
		} else {
			TDS_PUT_N_AS_UCS2(tds, "sp_executesql");
		}
		tds_put_smallint(tds, 0);

		tds7_put_query_params(tds, converted_query, converted_query_len);
		rc = tds7_write_param_def_from_query(tds, converted_query, converted_query_len, params);
		tds_convert_string_free(query, converted_query);
		if (TDS_FAILED(rc)) {
			tds_freeze_abort(&outer);
			return rc;
		}
		tds_freeze_close(&outer);

		for (i = 0; i < params->num_cols; i++) {
			param = params->columns[i];
			TDS_PROPAGATE(tds_put_data_info(tds, param, 0));
			TDS_PROPAGATE(tds_put_data(tds, param));
		}

		tds->current_op = TDS_OP_EXECUTESQL;
		return tds_query_flush_packet(tds);
	}

	/* allocate a structure for this thing */
	dyn = tds_alloc_dynamic(tds->conn, NULL);

	if (!dyn)
		return TDS_FAIL;
	/* check if no parameters */
	if (params && !params->num_cols)
		params = NULL;

	/* TDS 4.2, emulate prepared statements */
	/*
	 * TODO Sybase seems to not support parameters in prepared execdirect
	 * so use language or prepare and then exec
	 */
	if (!IS_TDS50(tds->conn) || params) {
		TDSRET ret = TDS_SUCCESS;

		if (!params) {
			ret = tds_submit_query(tds, query);
		} else {
			dyn->emulated = 1;
			dyn->params = params;
			dyn->query = strdup(query);
			if (!dyn->query)
				ret = TDS_FAIL;
			if (TDS_SUCCEED(ret))
				if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
					ret = TDS_FAIL;
			if (TDS_SUCCEED(ret)) {
				ret = tds_send_emulated_execute(tds, dyn->query, dyn->params);
				if (TDS_SUCCEED(ret))
					ret = tds_query_flush_packet(tds);
			}
			/* do not free our parameters */
			dyn->params = NULL;
		}
		tds_dynamic_deallocated(tds->conn, dyn);
		tds_release_dynamic(&dyn);
		return ret;
	}

	tds_release_cur_dyn(tds);
	tds->cur_dyn = dyn;

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds->out_flag = TDS_NORMAL;

	id_len = strlen(dyn->id);
	tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
	tds_freeze(tds, &outer, 2);
	tds_put_byte(tds, TDS_DYN_EXEC_IMMED);
	tds_put_byte(tds, params ? 0x01 : 0);
	TDS_START_LEN_TINYINT(tds) {
		tds_put_string(tds, dyn->id, id_len);
	} TDS_END_LEN
	/* TODO how to pass parameters type? like store procedures ? */
	TDS_START_LEN_USMALLINT(tds) {
		tds_put_n(tds, "create proc ", 12);
		tds_put_string(tds, dyn->id, id_len);
		tds_put_n(tds, " as ", 4);
		tds_put_string(tds, query, query_len);
	} TDS_END_LEN
	tds_freeze_close(&outer);

	if (params)
		TDS_PROPAGATE(tds5_put_params(tds, params, 0));

	return tds_flush_packet(tds);
}

/**
 * Creates a temporary stored procedure in the server and execute it.
 * \param tds     state information for the socket and the TDS protocol
 * \param query   language query with given placeholders ('?')
 * \param id      string to identify the dynamic query. Pass NULL for automatic generation.
 * \param dyn_out will receive allocated TDSDYNAMIC*. Any older allocated dynamic won't be freed. Can be NULL.
 * \param params  parameters to use. It can be NULL even if parameters are present.
 * \return TDS_FAIL or TDS_SUCCESS
 */
TDSRET
tds71_submit_prepexec(TDSSOCKET * tds, const char *query, const char *id, TDSDYNAMIC ** dyn_out, TDSPARAMINFO * params)
{
	int query_len;
	TDSRET rc = TDS_FAIL;
	TDSDYNAMIC *dyn;
	size_t converted_query_len;
	const char *converted_query;
	TDSFREEZE outer;

	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);

	if (!query || !dyn_out || !IS_TDS7_PLUS(tds->conn))
		return TDS_FAIL;

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	/* allocate a structure for this thing */
	dyn = tds_alloc_dynamic(tds->conn, id);
	if (!dyn)
		return TDS_FAIL;
	tds_release_dynamic(dyn_out);
	*dyn_out = dyn;

	tds_set_cur_dyn(tds, dyn);

	query_len = (int)strlen(query);

	converted_query = tds_convert_string(tds, tds->conn->char_convs[client2ucs2], query, query_len, &converted_query_len);
	if (!converted_query)
		goto failure;

	tds_freeze(tds, &outer, 0);
	tds_start_query(tds, TDS_RPC);
	/* procedure name */
	if (IS_TDS71_PLUS(tds->conn)) {
		tds_put_smallint(tds, -1);
		tds_put_smallint(tds, TDS_SP_PREPEXEC);
	} else {
		TDS_PUT_N_AS_UCS2(tds, "sp_prepexec");
	}
	tds_put_smallint(tds, 0);

	/* return param handle (int) */
	tds_put_byte(tds, 0);
	tds_put_byte(tds, 1);	/* result */
	tds_put_byte(tds, SYBINTN);
	tds_put_byte(tds, 4);
	tds_put_byte(tds, 0);

	rc = tds7_write_param_def_from_query(tds, converted_query, converted_query_len, params);
	tds7_put_query_params(tds, converted_query, converted_query_len);
	tds_convert_string_free(query, converted_query);
	if (TDS_FAILED(rc)) {
		tds_freeze_abort(&outer);
		return rc;
	}
	tds_freeze_close(&outer);

	if (params) {
		int i;

		for (i = 0; i < params->num_cols; i++) {
			TDSCOLUMN *param = params->columns[i];
			TDS_PROPAGATE(tds_put_data_info(tds, param, 0));
			TDS_PROPAGATE(tds_put_data(tds, param));
		}
	}

	tds->current_op = TDS_OP_PREPEXEC;

	rc = tds_query_flush_packet(tds);
	if (TDS_SUCCEED(rc))
		return rc;

failure:
	/* TODO correct if writing fail ?? */
	tds_set_state(tds, TDS_IDLE);

	tds_release_dynamic(dyn_out);
	tds_dynamic_deallocated(tds->conn, dyn);
	return rc;
}

/**
 * Get column size for wire
 */
size_t
tds_fix_column_size(TDSSOCKET * tds TDS_UNUSED, TDSCOLUMN * curcol)
{
	size_t size = curcol->on_server.column_size, min;

	if (!size) {
		size = curcol->column_size;
		if (is_unicode_type(curcol->on_server.column_type))
			size *= 2u;
	}

	switch (curcol->column_varint_size) {
	case 1:
		size = MAX(MIN(size, 255), 1);
		break;
	case 2:
		/* note that varchar(max)/varbinary(max) have a varint of 8 */
		if (curcol->on_server.column_type == XSYBNVARCHAR || curcol->on_server.column_type == XSYBNCHAR)
			min = 2;
		else
			min = 1;
		size = MAX(MIN(size, 8000u), min);
		break;
	case 4:
		if (curcol->on_server.column_type == SYBNTEXT)
			size = 0x7ffffffeu;
		else
			size = 0x7fffffffu;
		break;
	default:
		break;
	}
	return size;
}

/**
 * Put data information to wire
 * \param tds    state information for the socket and the TDS protocol
 * \param curcol column where to store information
 * \param flags  bit flags on how to send data (use TDS_PUT_DATA_USE_NAME for use name information)
 * \return TDS_SUCCESS or TDS_FAIL
 */
static TDSRET
tds_put_data_info(TDSSOCKET * tds, TDSCOLUMN * curcol, int flags)
{
	int len;

	CHECK_TDS_EXTRA(tds);
	CHECK_COLUMN_EXTRA(curcol);

	if (flags & TDS_PUT_DATA_USE_NAME) {
		len = tds_dstr_len(&curcol->column_name);
		tdsdump_log(TDS_DBG_ERROR, "tds_put_data_info putting param_name \n");

		if (IS_TDS7_PLUS(tds->conn)) {
			TDSFREEZE outer;
			size_t written;

			tds_freeze(tds, &outer, 1);
			if ((flags & TDS_PUT_DATA_PREFIX_NAME) != 0)
				tds_put_smallint(tds, '@');
			tds_put_string(tds, tds_dstr_cstr(&curcol->column_name), len);
			written = (tds_freeze_written(&outer) - 1) / 2;
			tds_freeze_close_len(&outer, written);
		} else {
			TDS_START_LEN_TINYINT(tds) { /* param name len */
				tds_put_string(tds, tds_dstr_cstr(&curcol->column_name), len);
			} TDS_END_LEN
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
	if (flags & TDS_PUT_DATA_LONG_STATUS)
		tds_put_int(tds, curcol->column_output);	/* status (input) */
	else
		tds_put_byte(tds, curcol->column_output);	/* status (input) */
	if (!IS_TDS7_PLUS(tds->conn))
		tds_put_int(tds, curcol->column_usertype);	/* usertype */
	tds_put_byte(tds, curcol->on_server.column_type);

	if (curcol->funcs->put_info(tds, curcol) != TDS_SUCCESS)
		return TDS_FAIL;

	/* TODO needed in TDS4.2 ?? now is called only if TDS >= 5 */
	if (!IS_TDS7_PLUS(tds->conn))
		tds_put_byte(tds, 0x00);	/* locale info length */

	return TDS_SUCCESS;
}

/**
 * Send dynamic request on TDS 7+ to be executed
 * \tds
 * \param dyn  dynamic query to execute
 */
static TDSRET
tds7_send_execute(TDSSOCKET * tds, TDSDYNAMIC * dyn)
{
	TDSCOLUMN *param;
	TDSPARAMINFO *info;
	int i;

	/* procedure name */
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
			TDS_PROPAGATE(tds_put_data_info(tds, param, 0));
			TDS_PROPAGATE(tds_put_data(tds, param));
		}

	tds->current_op = TDS_OP_EXECUTE;
	return TDS_SUCCESS;
}

/**
 * Sends a previously prepared dynamic statement to the server.
 * \param tds state information for the socket and the TDS protocol
 * \param dyn dynamic proc to execute. Must build from same tds.
 */
TDSRET
tds_submit_execute(TDSSOCKET * tds, TDSDYNAMIC * dyn)
{
	CHECK_TDS_EXTRA(tds);
	/* TODO this dynamic should be in tds */
	CHECK_DYNAMIC_EXTRA(dyn);

	tdsdump_log(TDS_DBG_FUNC, "tds_submit_execute()\n");

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_set_cur_dyn(tds, dyn);

	if (IS_TDS7_PLUS(tds->conn)) {
		/* check proper id */
		if (dyn->num_id == 0) {
			tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}

		/* RPC on sp_execute */
		tds_start_query(tds, TDS_RPC);

		tds7_send_execute(tds, dyn);

		return tds_query_flush_packet(tds);
	}

	if (dyn->emulated) {
		TDS_PROPAGATE(tds_send_emulated_execute(tds, dyn->query, dyn->params));
		return tds_query_flush_packet(tds);
	}

	/* query has been prepared successfully, discard original query */
	if (dyn->query)
		TDS_ZERO_FREE(dyn->query);

	tds->out_flag = TDS_NORMAL;
	/* dynamic id */
	tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
	TDS_START_LEN_USMALLINT(tds) {
		tds_put_byte(tds, 0x02);
		tds_put_byte(tds, dyn->params ? 0x01 : 0);
		TDS_START_LEN_TINYINT(tds) {
			tds_put_string(tds, dyn->id, -1);
		} TDS_END_LEN
		tds_put_smallint(tds, 0);
	} TDS_END_LEN

	if (dyn->params)
		TDS_PROPAGATE(tds5_put_params(tds, dyn->params, 0));

	/* send it */
	return tds_query_flush_packet(tds);
}

/**
 * Send parameters to server.
 * \tds
 * \param info   parameters to send
 * \param flags  0 or TDS_PUT_DATA_USE_NAME
 */
static TDSRET
tds5_put_params(TDSSOCKET * tds, TDSPARAMINFO * info, int flags)
{
	int i;
	bool wide = false;

	CHECK_TDS_EXTRA(tds);
	CHECK_PARAMINFO_EXTRA(info);

	/* column descriptions */
	for (;;) {
		TDSFREEZE outer, inner;

		tds_freeze(tds, &outer, 0);
		if (wide) {
			tds_put_byte(tds, TDS5_PARAMFMT2_TOKEN);
			tds_freeze(tds, &inner, 4);
			flags |= TDS_PUT_DATA_LONG_STATUS;
		} else {
			tds_put_byte(tds, TDS5_PARAMFMT_TOKEN);
			tds_freeze(tds, &inner, 2);
		}

		/* number of parameters */
		tds_put_smallint(tds, info->num_cols);

		/* column detail for each parameter */
		for (i = 0; i < info->num_cols; i++)
			TDS_PROPAGATE(tds_put_data_info(tds, info->columns[i], flags));

		/* if we fits we are fine */
		if (wide || tds_freeze_written(&inner) - 2 < 0x10000u) {
			tds_freeze_close(&inner);
			tds_freeze_close(&outer);
			break;
		}

		/* try again with wide */
		tds_freeze_abort(&inner);
		tds_freeze_abort(&outer);
		if (!tds_capability_has_req(tds->conn, TDS_REQ_WIDETABLE))
			return TDS_FAIL;
		wide = true;
	}

	/* row data */
	tds_put_byte(tds, TDS5_PARAMS_TOKEN);
	for (i = 0; i < info->num_cols; i++)
		TDS_PROPAGATE(tds_put_data(tds, info->columns[i]));
	return TDS_SUCCESS;
}

/**
 * Check if dynamic request must be unprepared.
 * Depending on status and protocol version request should be unprepared
 * or not.
 * \tds
 * \param dyn  dynamic request to check
 */
int
tds_needs_unprepare(TDSCONNECTION * conn, TDSDYNAMIC * dyn)
{
	CHECK_CONN_EXTRA(conn);
	CHECK_DYNAMIC_EXTRA(dyn);

	/* check if statement is prepared */
	if (IS_TDS7_PLUS(conn) && !dyn->num_id)
		return 0;

	if (dyn->emulated || !dyn->id[0])
		return 0;

	return 1;
}

/**
 * Unprepare dynamic on idle.
 * This let libTDS close the prepared statement when possible.
 * \tds
 * \param dyn  dynamic request to close
 */
TDSRET
tds_deferred_unprepare(TDSCONNECTION * conn, TDSDYNAMIC * dyn)
{
	CHECK_CONN_EXTRA(conn);
	CHECK_DYNAMIC_EXTRA(dyn);

	if (!tds_needs_unprepare(conn, dyn)) {
		tds_dynamic_deallocated(conn, dyn);
		return TDS_SUCCESS;
	}

	dyn->defer_close = true;
	conn->pending_close = 1;

	return TDS_SUCCESS;
}

/**
 * Send a unprepare request for a prepared query
 * \param tds state information for the socket and the TDS protocol
 * \param dyn dynamic query
 * \result TDS_SUCCESS or TDS_FAIL
 */
TDSRET
tds_submit_unprepare(TDSSOCKET * tds, TDSDYNAMIC * dyn)
{
	CHECK_TDS_EXTRA(tds);
	/* TODO test dyn in tds */
	CHECK_DYNAMIC_EXTRA(dyn);

	if (!dyn)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_FUNC, "tds_submit_unprepare() %s\n", dyn->id);

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_set_cur_dyn(tds, dyn);

	if (IS_TDS7_PLUS(tds->conn)) {
		/* RPC on sp_execute */
		tds_start_query(tds, TDS_RPC);

		/* procedure name */
		if (IS_TDS71_PLUS(tds->conn)) {
			/* save some byte for mssql2k */
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_UNPREPARE);
		} else {
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

		tds->current_op = TDS_OP_UNPREPARE;
		return tds_query_flush_packet(tds);
	}

	if (dyn->emulated) {
		tds_start_query(tds, TDS_QUERY);

		/* just a dummy select to return some data */
		tds_put_string(tds, "select 1 where 0=1", -1);
		return tds_query_flush_packet(tds);
	}

	tds->out_flag = TDS_NORMAL;
	/* dynamic id */
	tds_put_byte(tds, TDS5_DYNAMIC_TOKEN);
	TDS_START_LEN_USMALLINT(tds) {
		tds_put_byte(tds, TDS_DYN_DEALLOC);
		tds_put_byte(tds, 0x00);
		TDS_START_LEN_TINYINT(tds) {
			tds_put_string(tds, dyn->id, -1);
		} TDS_END_LEN
		tds_put_smallint(tds, 0);
	} TDS_END_LEN

	/* send it */
	tds->current_op = TDS_OP_DYN_DEALLOC;
	return tds_query_flush_packet(tds);
}

/**
 * Send RPC as string query.
 * This function is used on old protocol which does not support RPC queries.
 * \tds
 * \param rpc_name  name of RPC to invoke
 * \param params    parameters to send to server
 * \returns TDS_FAIL or TDS_SUCCESS
 */
static TDSRET
tds4_send_emulated_rpc(TDSSOCKET * tds, const char *rpc_name, TDSPARAMINFO * params)
{
	TDSCOLUMN *param;
	int i, n;
	int num_params = params ? params->num_cols : 0;
	const char *sep = " ";
	char buf[80];

	/* create params and set */
	for (i = 0, n = 0; i < num_params; ++i) {

		param = params->columns[i];

		/* declare and set output parameters */
		if (!param->column_output)
			continue;
		++n;
		sprintf(buf, " DECLARE @P%d ", n);
		tds_get_column_declaration(tds, param, buf + strlen(buf));
		sprintf(buf + strlen(buf), " SET @P%d=", n);
		tds_put_string(tds, buf, -1);
		tds_put_param_as_string(tds, params, i);
	}

	/* put exec statement */
	tds_put_string(tds, " EXEC ", 6);
	tds_put_string(tds, rpc_name, -1);

	/* put arguments */
	for (i = 0, n = 0; i < num_params; ++i) {
		param = params->columns[i];
		tds_put_string(tds, sep, -1);
		if (!tds_dstr_isempty(&param->column_name)) {
			tds_put_string(tds, tds_dstr_cstr(&param->column_name), tds_dstr_len(&param->column_name));
			tds_put_string(tds, "=", 1);
		}
		if (param->column_output) {
			++n;
			sprintf(buf, "@P%d OUTPUT", n);
			tds_put_string(tds, buf, -1);
		} else {
			tds_put_param_as_string(tds, params, i);
		}
		sep = ",";
	}

	return tds_query_flush_packet(tds);
}

/**
 * Calls a RPC from server. Output parameters will be stored in tds->param_info.
 * \param tds      state information for the socket and the TDS protocol
 * \param rpc_name name of RPC
 * \param params   parameters information. NULL for no parameters
 */
TDSRET
tds_submit_rpc(TDSSOCKET * tds, const char *rpc_name, TDSPARAMINFO * params, TDSHEADERS * head)
{
	TDSCOLUMN *param;
	int rpc_name_len, i;
	int num_params = params ? params->num_cols : 0;

	CHECK_TDS_EXTRA(tds);
	if (params)
		CHECK_PARAMINFO_EXTRA(params);

	assert(tds);
	assert(rpc_name);

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	/* distinguish from dynamic query  */
	tds_release_cur_dyn(tds);

	rpc_name_len = (int)strlen(rpc_name);
	if (IS_TDS7_PLUS(tds->conn)) {
		TDSFREEZE outer;
		size_t written;

		if (tds_start_query_head(tds, TDS_RPC, head) != TDS_SUCCESS)
			return TDS_FAIL;

		/* procedure name */
		tds_freeze(tds, &outer, 2);
		tds_put_string(tds, rpc_name, rpc_name_len);
		written = tds_freeze_written(&outer) / 2 - 1;
		tds_freeze_close_len(&outer, written);

		/*
		 * TODO support flags
		 * bit 0 (1 as flag) in TDS7/TDS5 is "recompile"
		 * bit 1 (2 as flag) in TDS7+ is "no metadata" bit 
		 * (I don't know meaning of "no metadata")
		 */
		tds_put_smallint(tds, 0);

		for (i = 0; i < num_params; i++) {
			param = params->columns[i];
			TDS_PROPAGATE(tds_put_data_info(tds, param, TDS_PUT_DATA_USE_NAME));
			TDS_PROPAGATE(tds_put_data(tds, param));
		}

		return tds_query_flush_packet(tds);
	}

	if (IS_TDS50(tds->conn)) {
		tds->out_flag = TDS_NORMAL;

		/* DBRPC */
		tds_put_byte(tds, TDS_DBRPC_TOKEN);
		TDS_START_LEN_USMALLINT(tds) {
			TDS_START_LEN_TINYINT(tds) {
				tds_put_string(tds, rpc_name, rpc_name_len);
			} TDS_END_LEN
			/* TODO flags */
			tds_put_smallint(tds, num_params ? 2 : 0);
		} TDS_END_LEN

		if (num_params)
			TDS_PROPAGATE(tds5_put_params(tds, params, TDS_PUT_DATA_USE_NAME));

		/* send it */
		return tds_query_flush_packet(tds);
	}

	/* emulate it for TDS4.x, send RPC for mssql */
	if (tds->conn->tds_version < 0x500)
		return tds4_send_emulated_rpc(tds, rpc_name, params);

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
TDSRET
tds_send_cancel(TDSSOCKET * tds)
{
#if ENABLE_ODBC_MARS
	CHECK_TDS_EXTRA(tds);

	tdsdump_log(TDS_DBG_FUNC, "tds_send_cancel: %sin_cancel and %sidle\n", 
				(tds->in_cancel? "":"not "), (tds->state == TDS_IDLE? "":"not "));

	/* one cancel is sufficient */
	if (tds->in_cancel || tds->state == TDS_IDLE) {
		return TDS_SUCCESS;
	}

	tds->in_cancel = 1;

	if (tds_mutex_trylock(&tds->conn->list_mtx)) {
		/* TODO check */
		/* signal other socket */
		tds_wakeup_send(&tds->conn->wakeup, 1);
		return TDS_SUCCESS;
	}
	if (tds->conn->in_net_tds) {
		tds_mutex_unlock(&tds->conn->list_mtx);
		/* TODO check */
		/* signal other socket */
		tds_wakeup_send(&tds->conn->wakeup, 1);
		return TDS_SUCCESS;
	}
	tds_mutex_unlock(&tds->conn->list_mtx);

	/*
	problem: if we are in in_net and we got a signal ??
	on timeout we and a cancel, directly in in_net
	if we hold the lock and we get a signal lock create a death lock

	if we use a recursive mutex and we can get the lock there are 2 cases
	- same thread, we could add a packet and signal, no try ok
	- first thread locking, we could add a packet but are we sure it get processed ??, no try ok
	if recursive mutex and we can't get another thread, wait

	if mutex is not recursive and we get the lock (try)
	- nobody locked, if in_net it could be same or another
	if mutex is not recursive and we can't get the lock
	- another thread is locking, sending signal require not exiting and global list (not protected by list_mtx)
	- same thread have lock, we can't wait nothing without deathlock, setting a flag in tds and signaling could help

	if a tds is waiting for data or is waiting for a condition or for a signal in poll
	pass cancel request on socket ??
	 */

	tds->out_flag = TDS_CANCEL;
	tds->out_pos = 8;
 	tdsdump_log(TDS_DBG_FUNC, "tds_send_cancel: sending cancel packet\n");
	return tds_flush_packet(tds);
#else
	TDSRET rc;

	/*
	 * if we are not able to get the lock signal other thread
	 * this means that either:
	 * - another thread is processing data
	 * - we got called from a signal inside processing thread
	 * - we got called from message handler
	 */
	if (tds_mutex_trylock(&tds->wire_mtx)) {
		/* TODO check */
		if (!tds->in_cancel)
			tds->in_cancel = 1;
		/* signal other socket */
		tds_wakeup_send(&tds->conn->wakeup, 1);
		return TDS_SUCCESS;
	}

	CHECK_TDS_EXTRA(tds);

	tdsdump_log(TDS_DBG_FUNC, "tds_send_cancel: %sin_cancel and %sidle\n", 
				(tds->in_cancel? "":"not "), (tds->state == TDS_IDLE? "":"not "));

	/* one cancel is sufficient */
	if (tds->in_cancel || tds->state == TDS_IDLE) {
		tds_mutex_unlock(&tds->wire_mtx);
		return TDS_SUCCESS;
	}

	rc = tds_put_cancel(tds);
	tds_mutex_unlock(&tds->wire_mtx);

	return rc;
#endif
}

/**
 * Quote a string properly. Output string is always NUL-terminated
 * \param buffer   output buffer. If NULL function will just return
 *        required bytes
 * \param quoting  quote character (should be one of '\'', '"', ']')
 * \param id       string to quote
 * \param len      length of string to quote
 * \returns size of output string
 */
static size_t
tds_quote(char *buffer, char quoting, const char *id, size_t len)
{
	size_t size;
	const char *src, *pend;
	char *dst;

	pend = id + len;

	/* quote */
	src = id;
	if (!buffer) {
		size = 2u + len;
		for (; src != pend; ++src)
			if (*src == quoting)
				++size;
		return size;
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
 * \param idlen  id length (< 0 for NUL terminated)
 * \result written chars (not including needed terminator)
 * \see tds_quote_id_rpc
 */
size_t
tds_quote_id(TDSSOCKET * tds, char *buffer, const char *id, int idlen)
{
	size_t i, len;

	CHECK_TDS_EXTRA(tds);

	len = idlen < 0 ? strlen(id) : (size_t) idlen;

	/* quote always for mssql */
	if (TDS_IS_MSSQL(tds) || tds->conn->product_version >= TDS_SYB_VER(12, 5, 1))
		return tds_quote(buffer, ']', id, len);

	/* need quote ?? */
	for (i = 0; i < len; ++i) {
		char c = id[i];

		if (c >= 'a' && c <= 'z')
			continue;
		if (c >= 'A' && c <= 'Z')
			continue;
		if (i > 0 && c >= '0' && c <= '9')
			continue;
		if (c == '_')
			continue;
		return tds_quote(buffer, '\"', id, len);
	}

	if (buffer) {
		memcpy(buffer, id, len);
		buffer[len] = '\0';
	}
	return len;
}

/**
 * Quote an id for a RPC call
 * \param tds    state information for the socket and the TDS protocol
 * \param buffer buffer to store quoted id. If NULL do not write anything
 *        (useful to compute quote length)
 * \param id     id to quote
 * \param idlen  id length (< 0 for NUL terminated)
 * \result written chars (not including needed terminator)
 * \see tds_quote_id
 */
size_t
tds_quote_id_rpc(TDSSOCKET * tds, char *buffer, const char *id, int idlen)
{
	size_t len;
	/* We are quoting for RPC calls, not base language queries. For RPC calls Sybase
	 * servers don't accept '[]' style quoting so don't use them but use normal
	 * identifier quoting ('""') */
	char quote_id_char = TDS_IS_MSSQL(tds) ? ']' : '\"';

	CHECK_TDS_EXTRA(tds);

	len = idlen < 0 ? strlen(id) : (size_t) idlen;

	return tds_quote(buffer, quote_id_char, id, len);
}

/**
 * Quote a string
 * \param tds    state information for the socket and the TDS protocol
 * \param buffer buffer to store quoted id. If NULL do not write anything 
 *        (useful to compute quote length)
 * \param str    string to quote (not necessary NUL-terminated)
 * \param len    length of string (-1 for NUL-terminated)
 * \result written chars (not including needed terminator)
 */
size_t
tds_quote_string(TDSSOCKET * tds TDS_UNUSED, char *buffer, const char *str, int len)
{
	return tds_quote(buffer, '\'', str, len < 0 ? strlen(str) : (size_t) len);
}

/**
 * Set current cursor.
 * Current cursor is the one will receive output from server.
 * \tds
 * \param cursor  cursor to set as current
 */
static inline void
tds_set_cur_cursor(TDSSOCKET *tds, TDSCURSOR *cursor)
{
	++cursor->ref_count;
	if (tds->cur_cursor)
		tds_release_cursor(&tds->cur_cursor);
	tds->cur_cursor = cursor;
}

TDSRET
tds_cursor_declare(TDSSOCKET * tds, TDSCURSOR * cursor, bool *something_to_send)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_declare() cursor id = %d\n", cursor->cursor_id);

	if (IS_TDS7_PLUS(tds->conn)) {
		cursor->srv_status |= TDS_CUR_ISTAT_DECLARED;
		cursor->srv_status |= TDS_CUR_ISTAT_CLOSED;
		cursor->srv_status |= TDS_CUR_ISTAT_RDONLY;
	}

	if (IS_TDS50(tds->conn)) {
		if (!*something_to_send) {
			if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
				return TDS_FAIL;

			tds->out_flag = TDS_NORMAL;
		}
		if (tds->state != TDS_WRITING || tds->out_flag != TDS_NORMAL)
			return TDS_FAIL;

		tds_put_byte(tds, TDS_CURDECLARE_TOKEN);

		/* length of the data stream that follows */
		TDS_START_LEN_USMALLINT(tds) {
			TDS_START_LEN_TINYINT(tds) {
				tds_put_string(tds, cursor->cursor_name, -1);
			} TDS_END_LEN
			tds_put_byte(tds, 1);	/* cursor option is read only=1, unused=0 */
			tds_put_byte(tds, 0);	/* status unused=0 */
			TDS_START_LEN_USMALLINT(tds) {
				tds_put_string(tds, cursor->query, -1);
			} TDS_END_LEN
			tds_put_tinyint(tds, 0);	/* number of columns = 0 , valid value applicable only for updatable cursor */
		} TDS_END_LEN
		*something_to_send = true;
	}

	return TDS_SUCCESS;
}

TDSRET
tds_cursor_open(TDSSOCKET * tds, TDSCURSOR * cursor, TDSPARAMINFO *params, bool *something_to_send)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_open() cursor id = %d\n", cursor->cursor_id);

	if (!*something_to_send) {
		if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
			return TDS_FAIL;
	}
	if (tds->state != TDS_WRITING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	if (IS_TDS50(tds->conn)) {
		tds->out_flag = TDS_NORMAL;
		tds_put_byte(tds, TDS_CUROPEN_TOKEN);
		TDS_START_LEN_USMALLINT(tds) {

			/*tds_put_int(tds, cursor->cursor_id); *//* Only if cursor id is passed as zero, the cursor name need to be sent */

			tds_put_int(tds, 0);
			TDS_START_LEN_TINYINT(tds) {
				tds_put_string(tds, cursor->cursor_name, -1);
			} TDS_END_LEN
			tds_put_byte(tds, 0);	/* Cursor status : 0 for no arguments */
		} TDS_END_LEN
		*something_to_send = true;
	}
	if (IS_TDS7_PLUS(tds->conn)) {
		const char *converted_query;
		size_t converted_query_len;
		int num_params = params ? params->num_cols : 0;
		TDSFREEZE outer;
		TDSRET rc = TDS_SUCCESS;

		/* cursor statement */
		converted_query = tds_convert_string(tds, tds->conn->char_convs[client2ucs2],
						     cursor->query, (int)strlen(cursor->query), &converted_query_len);
		if (!converted_query) {
			if (!*something_to_send)
				tds_set_state(tds, TDS_IDLE);
			return TDS_FAIL;
		}

		tds_freeze(tds, &outer, 0);

		/* RPC call to sp_cursoropen */
		tds_start_query(tds, TDS_RPC);

		/* procedure identifier by number */

		if (IS_TDS71_PLUS(tds->conn)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_CURSOROPEN);
		} else {
			TDS_PUT_N_AS_UCS2(tds, "sp_cursoropen");
		}

		tds_put_smallint(tds, 0);	/* flags */

		/* return cursor handle (int) */

		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 1);	/* output parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 0);

		if (num_params) {
			tds7_put_query_params(tds, converted_query, converted_query_len);
		} else {
			tds_put_byte(tds, 0);
			tds_put_byte(tds, 0);
			tds_put_byte(tds, SYBNTEXT);	/* must be Ntype */
			TDS_PUT_INT(tds, converted_query_len);
			if (IS_TDS71_PLUS(tds->conn))
				tds_put_n(tds, tds->conn->collation, 5);
			TDS_PUT_INT(tds, converted_query_len);
			tds_put_n(tds, converted_query, (int)converted_query_len);
		}

		/* type */
		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 1);	/* output parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, num_params ? cursor->type | 0x1000 : cursor->type);

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

		if (num_params) {
			int i;

			rc = tds7_write_param_def_from_query(tds, converted_query, converted_query_len, params);

			for (i = 0; i < num_params; i++) {
				TDSCOLUMN *param = params->columns[i];
				/* TODO check error */
				tds_put_data_info(tds, param, 0);
				/* FIXME handle error */
				tds_put_data(tds, param);
			}
		}
		tds_convert_string_free(cursor->query, converted_query);
		if (TDS_FAILED(rc)) {
			tds_freeze_abort(&outer);
			if (!*something_to_send)
				tds_set_state(tds, TDS_IDLE);
			return rc;
		}
		tds_freeze_close(&outer);

		*something_to_send = true;
		tds->current_op = TDS_OP_CURSOROPEN;
		tdsdump_log(TDS_DBG_ERROR, "tds_cursor_open (): RPC call set up \n");
	}


	tdsdump_log(TDS_DBG_ERROR, "tds_cursor_open (): cursor open completed\n");
	return TDS_SUCCESS;
}

TDSRET
tds_cursor_setrows(TDSSOCKET * tds, TDSCURSOR * cursor, bool *something_to_send)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_setrows() cursor id = %d\n", cursor->cursor_id);

	if (IS_TDS7_PLUS(tds->conn)) {
		cursor->srv_status &= ~TDS_CUR_ISTAT_DECLARED;
		cursor->srv_status |= TDS_CUR_ISTAT_CLOSED;
		cursor->srv_status |= TDS_CUR_ISTAT_ROWCNT;
	}

	if (IS_TDS50(tds->conn)) {
		if (!*something_to_send) {
			if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
				return TDS_FAIL;

			tds->out_flag = TDS_NORMAL;
		}
		if (tds->state != TDS_WRITING  || tds->out_flag != TDS_NORMAL)
			return TDS_FAIL;

		tds_set_cur_cursor(tds, cursor);
		tds_put_byte(tds, TDS_CURINFO_TOKEN);

		TDS_START_LEN_USMALLINT(tds) {
			/* length of data stream that follows */

			/* tds_put_int(tds, tds->cursor->cursor_id); */ /* Cursor id */

			tds_put_int(tds, 0);
			TDS_START_LEN_TINYINT(tds) {
				tds_put_string(tds, cursor->cursor_name, -1);
			} TDS_END_LEN
			tds_put_byte(tds, 1);	/* Command  TDS_CUR_CMD_SETCURROWS */
			tds_put_byte(tds, 0x00);	/* Status - TDS_CUR_ISTAT_ROWCNT 0x0020 */
			tds_put_byte(tds, 0x20);	/* Status - TDS_CUR_ISTAT_ROWCNT 0x0020 */
			tds_put_int(tds, cursor->cursor_rows);	/* row count to set */
		} TDS_END_LEN
		*something_to_send = true;

	}
	return TDS_SUCCESS;
}

static void
tds7_put_cursor_fetch(TDSSOCKET * tds, TDS_INT cursor_id, TDS_TINYINT fetch_type, TDS_INT i_row, TDS_INT num_rows)
{
	if (IS_TDS71_PLUS(tds->conn)) {
		tds_put_smallint(tds, -1);
		tds_put_smallint(tds, TDS_SP_CURSORFETCH);
	} else {
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

TDSRET
tds_cursor_fetch(TDSSOCKET * tds, TDSCURSOR * cursor, TDS_CURSOR_FETCH fetch_type, TDS_INT i_row)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_fetch() cursor id = %d\n", cursor->cursor_id);

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	if (IS_TDS50(tds->conn)) {
		size_t len = strlen(cursor->cursor_name);
		size_t row_len = 0;

		tds->out_flag = TDS_NORMAL;
		tds_put_byte(tds, TDS_CURFETCH_TOKEN);

		if (len > (255-10))
			len = (255-10);
		if (fetch_type == TDS_CURSOR_FETCH_ABSOLUTE || fetch_type == TDS_CURSOR_FETCH_RELATIVE)
			row_len = 4;

		/*tds_put_smallint(tds, 8); */

		TDS_PUT_SMALLINT(tds, 6 + len + row_len);	/* length of the data stream that follows */

		/*tds_put_int(tds, cursor->cursor_id); *//* cursor id returned by the server */

		tds_put_int(tds, 0);
		TDS_PUT_BYTE(tds, len);
		tds_put_n(tds, cursor->cursor_name, len);
		tds_put_tinyint(tds, fetch_type);

		/* optional argument to fetch row at absolute/relative position */
		if (row_len)
			tds_put_int(tds, i_row);
		return tds_query_flush_packet(tds);
	}

	if (IS_TDS7_PLUS(tds->conn)) {

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

		tds_start_query(tds, TDS_RPC);

		/* TODO enum for 2 ... */
		if (cursor->type == 2 && fetch_type == TDS_CURSOR_FETCH_ABSOLUTE) {
			/* strangely dynamic cursor do not support absolute so emulate it with first + relative */
			tds7_put_cursor_fetch(tds, cursor->cursor_id, 1, 0, 0);
			/* TODO define constant */
			tds_put_byte(tds, IS_TDS72_PLUS(tds->conn) ? 0xff : 0x80);
			tds7_put_cursor_fetch(tds, cursor->cursor_id, 0x20, i_row, cursor->cursor_rows);
		} else {
			/* TODO check fetch_type ?? */
			tds7_put_cursor_fetch(tds, cursor->cursor_id, mssql_fetch[fetch_type], i_row, cursor->cursor_rows);
		}

		tds->current_op = TDS_OP_CURSORFETCH;
		return tds_query_flush_packet(tds);
	}

	tds_set_state(tds, TDS_IDLE);
	return TDS_SUCCESS;
}

TDSRET
tds_cursor_get_cursor_info(TDSSOCKET *tds, TDSCURSOR *cursor, TDS_UINT *prow_number, TDS_UINT *prow_count)
{
	int done_flags;
	TDSRET retcode;
	TDS_INT result_type;

	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_get_cursor_info() cursor id = %d\n", cursor->cursor_id);

	/* Assume not known */
	assert(prow_number && prow_count);
	*prow_number = 0;
	*prow_count = 0;

	if (IS_TDS7_PLUS(tds->conn)) {
		/* Change state to querying */
		if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
			return TDS_FAIL;

		/* Remember the server has been sent a command for this cursor */
		tds_set_cur_cursor(tds, cursor);

		/* General initialization of server command */
		tds_start_query(tds, TDS_RPC);

		/* Create and send query to server */
		if (IS_TDS71_PLUS(tds->conn)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_CURSORFETCH);
		} else {
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
		tds_put_int(tds, cursor->cursor_id);

		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 0);	/* input parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 4);
		tds_put_int(tds, 0x100);	/* FETCH_INFO */

		/* row number */
		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 1);	/* output parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 0);

		/* number of rows fetched */
		tds_put_byte(tds, 0);	/* no parameter name */
		tds_put_byte(tds, 1);	/* output parameter  */
		tds_put_byte(tds, SYBINTN);
		tds_put_byte(tds, 4);
		tds_put_byte(tds, 0);

		/* Adjust current state */
		tds->current_op = TDS_OP_NONE;
		TDS_PROPAGATE(tds_query_flush_packet(tds));

		/* Process answer from server */
		for (;;) {
			retcode = tds_process_tokens(tds, &result_type, &done_flags, TDS_RETURN_PROC);
			tdsdump_log(TDS_DBG_FUNC, "tds_cursor_get_cursor_info: tds_process_tokens returned %d\n", retcode);
			tdsdump_log(TDS_DBG_FUNC, "    result_type=%d, TDS_DONE_COUNT=%x, TDS_DONE_ERROR=%x\n", 
						  result_type, (done_flags & TDS_DONE_COUNT), (done_flags & TDS_DONE_ERROR));
			switch (retcode) {
			case TDS_NO_MORE_RESULTS:
				return TDS_SUCCESS;
			case TDS_SUCCESS:
				if (result_type==TDS_PARAM_RESULT) {
					/* Status is updated when TDS_STATUS_RESULT token arrives, before the params are processed */
					if (tds->has_status && tds->ret_status==0) {
						TDSPARAMINFO *pinfo = tds->current_results;

						/* Make sure the params retuned have the correct type and size */
						if (pinfo && pinfo->num_cols==2
							  && pinfo->columns[0]->on_server.column_type==SYBINTN
							  && pinfo->columns[1]->on_server.column_type==SYBINTN
							  && pinfo->columns[0]->column_size==4
							  && pinfo->columns[1]->column_size==4) {
							/* Take the values */
							*prow_number = (TDS_UINT)(*(TDS_INT *) pinfo->columns[0]->column_data);
							*prow_count  = (TDS_UINT)(*(TDS_INT *) pinfo->columns[1]->column_data);
							tdsdump_log(TDS_DBG_FUNC, "----------------> prow_number=%u, prow_count=%u\n", 
										  *prow_count, *prow_number);
						}
					}
				}
				break;
			default:
				return retcode;
			}
		}
	}

	return TDS_SUCCESS;
}

TDSRET
tds_cursor_close(TDSSOCKET * tds, TDSCURSOR * cursor)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_close() cursor id = %d\n", cursor->cursor_id);

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	if (IS_TDS50(tds->conn)) {
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
	if (IS_TDS7_PLUS(tds->conn)) {

		/* RPC call to sp_cursorclose */
		tds_start_query(tds, TDS_RPC);

		if (IS_TDS71_PLUS(tds->conn)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_CURSORCLOSE);
		} else {
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
		tds->current_op = TDS_OP_CURSORCLOSE;
	}
	return tds_query_flush_packet(tds);

}

TDSRET
tds_cursor_setname(TDSSOCKET * tds, TDSCURSOR * cursor)
{
	TDSFREEZE outer;
	size_t written;

	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_setname() cursor id = %d\n", cursor->cursor_id);

	if (!IS_TDS7_PLUS(tds->conn))
		return TDS_SUCCESS;

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	/* RPC call to sp_cursoroption */
	tds_start_query(tds, TDS_RPC);

	if (IS_TDS71_PLUS(tds->conn)) {
		tds_put_smallint(tds, -1);
		tds_put_smallint(tds, TDS_SP_CURSOROPTION);
	} else {
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
	tds_put_byte(tds, XSYBNVARCHAR);
	tds_freeze(tds, &outer, 2);
	if (IS_TDS71_PLUS(tds->conn))
		tds_put_n(tds, tds->conn->collation, 5);
	TDS_START_LEN_USMALLINT(tds) {
		tds_put_string(tds, cursor->cursor_name, -1);
		written = tds_freeze_written(current_freeze) - 2;
	} TDS_END_LEN
	tds_freeze_close_len(&outer, written);

	tds->current_op = TDS_OP_CURSOROPTION;

	return tds_query_flush_packet(tds);
}

TDSRET
tds_cursor_update(TDSSOCKET * tds, TDSCURSOR * cursor, TDS_CURSOR_OPERATION op, TDS_INT i_row, TDSPARAMINFO *params)
{
	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_update() cursor id = %d\n", cursor->cursor_id);

	/* client must provide parameters for update */
	if (op == TDS_CURSOR_UPDATE && (!params || params->num_cols <= 0))
		return TDS_FAIL;

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_set_cur_cursor(tds, cursor);

	if (IS_TDS50(tds->conn)) {
		tds->out_flag = TDS_NORMAL;

		/* FIXME finish*/
		tds_set_state(tds, TDS_IDLE);
		return TDS_FAIL;
	}
	if (IS_TDS7_PLUS(tds->conn)) {

		/* RPC call to sp_cursorclose */
		tds_start_query(tds, TDS_RPC);

		if (IS_TDS71_PLUS(tds->conn)) {
			tds_put_smallint(tds, -1);
			tds_put_smallint(tds, TDS_SP_CURSOR);
		} else {
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
			TDSFREEZE outer;
			size_t written;

			/* empty table name */
			tds_put_byte(tds, 0);
			tds_put_byte(tds, 0);
			tds_put_byte(tds, XSYBNVARCHAR);
			num_params = params->num_cols;
			for (n = 0; n < num_params; ++n) {
				param = params->columns[n];
				if (!tds_dstr_isempty(&param->table_name)) {
					table_name = tds_dstr_cstr(&param->table_name);
					break;
				}
			}

			tds_freeze(tds, &outer, 2);
			if (IS_TDS71_PLUS(tds->conn))
				tds_put_n(tds, tds->conn->collation, 5);
			TDS_START_LEN_USMALLINT(tds) {
				if (table_name)
					tds_put_string(tds, table_name, -1);
				written = tds_freeze_written(current_freeze) - 2;
			} TDS_END_LEN
			tds_freeze_close_len(&outer, written);

			/* output columns to update */
			for (n = 0; n < num_params; ++n) {
				param = params->columns[n];
				/* TODO check error */
				tds_put_data_info(tds, param, TDS_PUT_DATA_USE_NAME|TDS_PUT_DATA_PREFIX_NAME);
				/* FIXME handle error */
				tds_put_data(tds, param);
			}
		}

		tds->current_op = TDS_OP_CURSOR;
	}
	return tds_query_flush_packet(tds);
}

/**
 * Check if a cursor is allocated into the server.
 * If is not allocated it assures is removed from the connection list
 * \tds
 * \return true if allocated false otherwise
 */
static bool
tds_cursor_check_allocated(TDSCONNECTION * conn, TDSCURSOR * cursor)
{
	if (cursor->srv_status == TDS_CUR_ISTAT_UNUSED || (cursor->srv_status & TDS_CUR_ISTAT_DEALLOC) != 0
	    || (IS_TDS7_PLUS(conn) && (cursor->srv_status & TDS_CUR_ISTAT_CLOSED) != 0)) {
		tds_cursor_deallocated(conn, cursor);
		return false;
	}

	return true;
}

/**
 * Send a deallocation request to server
 */
TDSRET
tds_cursor_dealloc(TDSSOCKET * tds, TDSCURSOR * cursor)
{
	TDSRET res = TDS_SUCCESS;

	CHECK_TDS_EXTRA(tds);

	if (!cursor)
		return TDS_FAIL;

	if (!tds_cursor_check_allocated(tds->conn, cursor))
		return TDS_SUCCESS;

	tdsdump_log(TDS_DBG_INFO1, "tds_cursor_dealloc() cursor id = %d\n", cursor->cursor_id);

	if (IS_TDS50(tds->conn)) {
		if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
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
	if (IS_TDS7_PLUS(tds->conn)) {
		if (cursor->status.dealloc == TDS_CURSOR_STATE_SENT ||
			cursor->status.dealloc == TDS_CURSOR_STATE_REQUESTED) {
			tdsdump_log(TDS_DBG_ERROR, "tds_cursor_dealloc(): freeing cursor \n");
		}
	}

	return res;
}

/**
 * Deallocate cursor on idle.
 * This let libTDS close the cursor when possible.
 * \tds
 * \param cursor   cursor to close
 */
TDSRET
tds_deferred_cursor_dealloc(TDSCONNECTION *conn, TDSCURSOR * cursor)
{
	CHECK_CONN_EXTRA(conn);
	CHECK_CURSOR_EXTRA(cursor);

	/* do not mark if already deallocated */
	if (!tds_cursor_check_allocated(conn, cursor))
		return TDS_SUCCESS;

	cursor->defer_close = true;
	conn->pending_close = 1;

	return TDS_SUCCESS;
}

/**
 * Send a string to server while quoting it.
 * \tds
 * \param s    string start
 * \param end  string end
 */
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

typedef struct tds_quoteout_stream {
	TDSOUTSTREAM stream;
	TDSSOCKET *tds;
	char buffer[2048];
} TDSQUOTEOUTSTREAM;

static int
tds_quoteout_stream_write(TDSOUTSTREAM *stream, size_t len)
{
	TDSQUOTEOUTSTREAM *s = (TDSQUOTEOUTSTREAM *) stream;
	TDSSOCKET *tds = s->tds;
	uint16_t buf[sizeof(s->buffer)];

	assert(len <= stream->buf_len);

#define QUOTE(type, ch) do { \
	type *src, *dst = (type *) buf, *end = (type *) (s->buffer + len); \
\
	for (src = (type *) s->buffer; src < end; ++src) { \
		if (*src == (ch)) \
			*dst++ = *src; \
		*dst++ = *src; \
	} \
	tds_put_n(tds, buf, (char *) dst - (char *) buf); \
} while(0)

	if (IS_TDS7_PLUS(tds->conn))
		QUOTE(uint16_t, TDS_HOST2LE('\''));
	else
		QUOTE(char, '\'');

#undef QUOTE

	return len;
}

static void
tds_quoteout_stream_init(TDSQUOTEOUTSTREAM * stream, TDSSOCKET * tds)
{
	stream->stream.write = tds_quoteout_stream_write;
	stream->stream.buffer = stream->buffer;
	stream->stream.buf_len = sizeof(stream->buffer);
	stream->tds = tds;
}

static TDSRET
tds_put_char_param_as_string(TDSSOCKET * tds, const TDSCOLUMN *curcol)
{
	TDS_CHAR *src;
	TDSICONV *char_conv = curcol->char_conv;
	int from, to;
	TDSSTATICINSTREAM r;
	TDSQUOTEOUTSTREAM w;

	src = (TDS_CHAR *) curcol->column_data;
	if (is_blob_col(curcol))
		src = ((TDSBLOB *)src)->textvalue;

	if (is_unicode_type(curcol->on_server.column_type))
		tds_put_string(tds, "N", 1);
	tds_put_string(tds, "\'", 1);

	/* Compute proper characted conversion.
	 * The conversion should be to UTF16/UCS2 for MS SQL.
	 * Avoid double conversion, convert directly from client to server.
	 */
	from = char_conv ? char_conv->from.charset.canonic : tds->conn->char_convs[client2ucs2]->from.charset.canonic;
	to = tds->conn->char_convs[IS_TDS7_PLUS(tds->conn) ? client2ucs2 : client2server_chardata]->to.charset.canonic;
	if (!char_conv || char_conv->to.charset.canonic != to)
		char_conv = tds_iconv_get_info(tds->conn, from, to);
	if (!char_conv)
		return TDS_FAIL;

	tds_staticin_stream_init(&r, src, curcol->column_cur_size);
	tds_quoteout_stream_init(&w, tds);

	tds_convert_stream(tds, char_conv, to_server, &r.stream, &w.stream);

	tds_put_string(tds, "\'", 1);
	return TDS_SUCCESS;
}

/**
 * Send a parameter to server.
 * Parameters are converted to string and sent to server.
 * \tds
 * \param params   parameters structure
 * \param n        number of parameter to send
 * \returns TDS_FAIL or TDS_SUCCESS
 */
static TDSRET
tds_put_param_as_string(TDSSOCKET * tds, TDSPARAMINFO * params, int n)
{
	TDSCOLUMN *curcol = params->columns[n];
	CONV_RESULT cr;
	TDS_INT res;
	TDS_CHAR *src;
	int src_len = curcol->column_cur_size;

	int i;
	char buf[256];
	bool quote = false;

	CHECK_TDS_EXTRA(tds);
	CHECK_PARAMINFO_EXTRA(params);

	if (src_len < 0) {
		/* on TDS 4 TEXT/IMAGE cannot be NULL, use empty */
		if (!IS_TDS50_PLUS(tds->conn) && is_blob_type(curcol->on_server.column_type))
			tds_put_string(tds, "''", 2);
		else
			tds_put_string(tds, "NULL", 4);
		return TDS_SUCCESS;
	}

	if (is_char_type(curcol->on_server.column_type))
		return tds_put_char_param_as_string(tds, curcol);

	src = (TDS_CHAR *) curcol->column_data;
	if (is_blob_col(curcol))
		src = ((TDSBLOB *)src)->textvalue;

	/* we could try to use only tds_convert but is not good in all cases */
	switch (curcol->on_server.column_type) {
	/* binary/char, do conversion in line */
	case SYBBINARY: case SYBVARBINARY: case SYBIMAGE: case XSYBBINARY: case XSYBVARBINARY:
		tds_put_string(tds, "0x", 2);
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
	/* TODO date, use iso format */
	case SYBDATETIME:
	case SYBDATETIME4:
	case SYBDATETIMN:
	case SYBMSTIME:
	case SYBMSDATE:
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
	case SYBTIME:
	case SYBDATE:
	case SYB5BIGTIME:
	case SYB5BIGDATETIME:
		/* TODO use an ISO context */
	case SYBUNIQUE:
		quote = true;
	default:
		res = tds_convert(tds_get_ctx(tds), tds_get_conversion_type(curcol->on_server.column_type, curcol->column_size), src, src_len, SYBCHAR, &cr);
		if (res < 0)
			return TDS_FAIL;

		if (quote)
			tds_put_string(tds, "\'", 1);
		tds_quote_and_put(tds, cr.c, cr.c + res);
		if (quote)
			tds_put_string(tds, "\'", 1);
		free(cr.c);
	}
	return TDS_SUCCESS;
}

/**
 * Emulate prepared execute traslating to a normal language
 */
static TDSRET
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
	if (!num_placeholders) {
		tds_put_string(tds, query, -1);
		return TDS_SUCCESS;
	}

	s = query;
	for (i = 0;; ++i) {
		e = tds_next_placeholder(s);
		tds_put_string(tds, s, (int)(e ? e - s : -1));
		if (!e)
			break;
		/* now translate parameter in string */
		tds_put_param_as_string(tds, params, i);

		s = e + 1;
	}
	
	return TDS_SUCCESS;
}

enum { MUL_STARTED = 1 };

TDSRET
tds_multiple_init(TDSSOCKET *tds, TDSMULTIPLE *multiple, TDS_MULTIPLE_TYPE type, TDSHEADERS * head)
{
	unsigned char packet_type;
	multiple->type = type;
	multiple->flags = 0;

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	packet_type = TDS_QUERY;
	switch (type) {
	case TDS_MULTIPLE_QUERY:
		break;
	case TDS_MULTIPLE_EXECUTE:
	case TDS_MULTIPLE_RPC:
		if (IS_TDS7_PLUS(tds->conn))
			packet_type = TDS_RPC;
		break;
	}
	if (tds_start_query_head(tds, packet_type, head) != TDS_SUCCESS)
		return TDS_FAIL;

	return TDS_SUCCESS;
}

TDSRET
tds_multiple_done(TDSSOCKET *tds, TDSMULTIPLE *multiple)
{
	assert(tds && multiple);

	return tds_query_flush_packet(tds);
}

TDSRET
tds_multiple_query(TDSSOCKET *tds, TDSMULTIPLE *multiple, const char *query, TDSPARAMINFO * params)
{
	assert(multiple->type == TDS_MULTIPLE_QUERY);

	if (multiple->flags & MUL_STARTED)
		tds_put_string(tds, " ", 1);
	multiple->flags |= MUL_STARTED;

	return tds_send_emulated_execute(tds, query, params);
}

TDSRET
tds_multiple_execute(TDSSOCKET *tds, TDSMULTIPLE *multiple, TDSDYNAMIC * dyn)
{
	assert(multiple->type == TDS_MULTIPLE_EXECUTE);

	if (IS_TDS7_PLUS(tds->conn)) {
		if (multiple->flags & MUL_STARTED) {
			/* TODO define constant */
			tds_put_byte(tds, IS_TDS72_PLUS(tds->conn) ? 0xff : 0x80);
		}
		multiple->flags |= MUL_STARTED;

		tds7_send_execute(tds, dyn);

		return TDS_SUCCESS;
	}

	if (multiple->flags & MUL_STARTED)
		tds_put_string(tds, " ", 1);
	multiple->flags |= MUL_STARTED;

	return tds_send_emulated_execute(tds, dyn->query, dyn->params);
}

/**
 * Send option commands to server.
 * Option commands are used to change server options.
 * \tds
 * \param command  command type.
 * \param option   option to set/get.
 * \param param    parameter value
 * \param param_size  length of parameter value in bytes
 */
TDSRET
tds_submit_optioncmd(TDSSOCKET * tds, TDS_OPTION_CMD command, TDS_OPTION option, TDS_OPTION_ARG *param, TDS_INT param_size)
{
	char cmd[128];
 
	CHECK_TDS_EXTRA(tds);
 
	tdsdump_log(TDS_DBG_FUNC, "tds_submit_optioncmd() \n");
 
	if (IS_TDS50(tds->conn)) {
		if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
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
 
		TDS_PROPAGATE(tds_process_simple_query(tds));
		return TDS_SUCCESS;
	}
 
	if (!IS_TDS7_PLUS(tds->conn))
		return TDS_SUCCESS;

	cmd[0] = 0;
	if (command == TDS_OPT_SET) {
		char datefmt[4];

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
		TDS_PROPAGATE(tds_process_simple_query(tds));
	}
	if (command == TDS_OPT_LIST) {
		int optionval = 0;
		TDS_INT resulttype;

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
		while (tds_process_tokens(tds, &resulttype, NULL, TDS_TOKEN_RESULTS) == TDS_SUCCESS) {
			switch (resulttype) {
			case TDS_ROWFMT_RESULT:
				break;
			case TDS_ROW_RESULT:
				while (tds_process_tokens(tds, &resulttype, NULL, TDS_STOPAT_ROWFMT|TDS_RETURN_DONE|TDS_RETURN_ROW) == TDS_SUCCESS) {
					TDSCOLUMN *col;
					CONV_RESULT dres;
					int ctype;
					unsigned char* src;
					int srclen;

					if (resulttype != TDS_ROW_RESULT)
						break;

					if (!tds->current_results)
						continue;

					col = tds->current_results->columns[0];
					ctype = tds_get_conversion_type(col->on_server.column_type, col->column_size);

					src = col->column_data;
					srclen = col->column_cur_size;


					tds_convert(tds_get_ctx(tds), ctype, src, srclen, SYBINT4, &dres);
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
	return TDS_SUCCESS;
}


/**
 * Send a rollback request.
 * TDS 7.2+ need this in order to handle transactions correctly if MARS is used.
 * \tds
 * \sa tds_submit_commit, tds_submit_rollback
 */
TDSRET
tds_submit_begin_tran(TDSSOCKET *tds)
{
	CHECK_TDS_EXTRA(tds);

	if (!IS_TDS72_PLUS(tds->conn))
		return tds_submit_query(tds, "BEGIN TRANSACTION");

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_start_query(tds, TDS7_TRANS);

	/* begin transaction */
	tds_put_smallint(tds, 5);
	tds_put_byte(tds, 0);	/* new transaction level TODO */
	tds_put_byte(tds, 0);	/* new transaction name */

	return tds_query_flush_packet(tds);
}

/**
 * Send a rollback request.
 * TDS 7.2+ need this in order to handle transactions correctly if MARS is used.
 * \tds
 * \param cont true to start a new transaction
 * \sa tds_submit_begin_tran, tds_submit_commit
 */
TDSRET
tds_submit_rollback(TDSSOCKET *tds, bool cont)
{
	CHECK_TDS_EXTRA(tds);

	if (!IS_TDS72_PLUS(tds->conn))
		return tds_submit_query(tds, cont ? "IF @@TRANCOUNT > 0 ROLLBACK BEGIN TRANSACTION" : "IF @@TRANCOUNT > 0 ROLLBACK");

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_start_query(tds, TDS7_TRANS);
	tds_put_smallint(tds, 8);	/* rollback */
	tds_put_byte(tds, 0);	/* name */
	if (cont) {
		tds_put_byte(tds, 1);
		tds_put_byte(tds, 0);	/* new transaction level TODO */
		tds_put_byte(tds, 0);	/* new transaction name */
	} else {
		tds_put_byte(tds, 0);	/* do not continue */
	}
	return tds_query_flush_packet(tds);
}

/**
 * Send a commit request.
 * TDS 7.2+ need this in order to handle transactions correctly if MARS is used.
 * \tds
 * \param cont true to start a new transaction
 * \sa tds_submit_rollback, tds_submit_begin_tran
 */
TDSRET
tds_submit_commit(TDSSOCKET *tds, bool cont)
{
	CHECK_TDS_EXTRA(tds);

	if (!IS_TDS72_PLUS(tds->conn))
		return tds_submit_query(tds, cont ? "IF @@TRANCOUNT > 0 COMMIT BEGIN TRANSACTION" : "IF @@TRANCOUNT > 0 COMMIT");

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING)
		return TDS_FAIL;

	tds_start_query(tds, TDS7_TRANS);
	tds_put_smallint(tds, 7);	/* commit */
	tds_put_byte(tds, 0);	/* name */
	if (cont) {
		tds_put_byte(tds, 1);
		tds_put_byte(tds, 0);	/* new transaction level TODO */
		tds_put_byte(tds, 0);	/* new transaction name */
	} else {
		tds_put_byte(tds, 0);	/* do not continue */
	}
	return tds_query_flush_packet(tds);
}

static const TDSCONTEXT empty_ctx = {0};

TDSRET
tds_disconnect(TDSSOCKET * tds)
{
	TDS_INT old_timeout;
	const TDSCONTEXT *old_ctx;

	CHECK_TDS_EXTRA(tds);
 
	tdsdump_log(TDS_DBG_FUNC, "tds_disconnect() \n");
 
	if (!IS_TDS50(tds->conn))
		return TDS_SUCCESS;

	old_timeout = tds->query_timeout;
	old_ctx = tds_get_ctx(tds);

	/* avoid to stall forever */
	tds->query_timeout = 5;

	/* do not report errors to upper libraries */
	tds_set_ctx(tds, &empty_ctx);

	if (tds_set_state(tds, TDS_WRITING) != TDS_WRITING) {
		tds->query_timeout = old_timeout;
		tds_set_ctx(tds, old_ctx);
		return TDS_FAIL;
	}
 
	tds->out_flag = TDS_NORMAL;
	tds_put_byte(tds, TDS_LOGOUT_TOKEN);
	tds_put_byte(tds, 0);
 
	tds_query_flush_packet(tds);
 
	return tds_process_simple_query(tds);
}
 
/*
 * TODO add function to return type suitable for param
 * ie:
 * sybvarchar -> sybvarchar / xsybvarchar
 * sybint4 -> sybintn
 */

/** @} */
