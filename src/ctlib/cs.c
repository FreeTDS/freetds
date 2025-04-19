/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005 Brian Bruns
 * Copyright (C) 2015  Frediano Ziglio
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

#include <freetds/time.h>

#include <stdio.h>
#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/replacements.h>
#include <freetds/utils.h>

#include "cspublic.h"
#include "ctlib.h"

#undef cs_dt_crack

static CS_INT cs_diag_storemsg(CS_CONTEXT *context, CS_CLIENTMSG *message);
static CS_INT cs_diag_clearmsg(CS_CONTEXT *context, CS_INT type);
static CS_INT cs_diag_getmsg(CS_CONTEXT *context, CS_INT idx, CS_CLIENTMSG *message);
static CS_INT cs_diag_countmsg(CS_CONTEXT *context, CS_INT *count);

const char *
cs_prretcode(int retcode)
{
	static char unknown[24];
	
	tdsdump_log(TDS_DBG_FUNC, "cs_prretcode(%d)\n", retcode);

	switch(retcode) {
	case CS_SUCCEED:	return "CS_SUCCEED";
	case CS_FAIL:		return "CS_FAIL";
	case CS_MEM_ERROR:	return "CS_MEM_ERROR";
	case CS_PENDING:	return "CS_PENDING";
	case CS_QUIET:		return "CS_QUIET";
	case CS_BUSY:		return "CS_BUSY";
	case CS_INTERRUPT:	return "CS_INTERRUPT";
	case CS_BLK_HAS_TEXT:	return "CS_BLK_HAS_TEXT";
	case CS_CONTINUE:	return "CS_CONTINUE";
	case CS_FATAL:		return "CS_FATAL";
	case CS_RET_HAFAILOVER:	return "CS_RET_HAFAILOVER";
	case CS_UNSUPPORTED:	return "CS_UNSUPPORTED";

	case CS_CANCELED:	return "CS_CANCELED";
	case CS_ROW_FAIL:	return "CS_ROW_FAIL";
	case CS_END_DATA:	return "CS_END_DATA";
	case CS_END_RESULTS:	return "CS_END_RESULTS";
	case CS_END_ITEM:	return "CS_END_ITEM";
	case CS_NOMSG:		return "CS_NOMSG";
	case CS_TIMED_OUT:	return "CS_TIMED_OUT";

	default:
		sprintf(unknown, "oops: %u ??", retcode);
	}
	return unknown;
}

static const char *
_cs_get_layer(int layer)
{
	tdsdump_log(TDS_DBG_FUNC, "_cs_get_layer(%d)\n", layer);

	switch (layer) {
	case 2:
		return "cslib user api layer";
		break;
	default:
		break;
	}
	return "unrecognized layer";
}

static const char *
_cs_get_origin(int origin)
{
	tdsdump_log(TDS_DBG_FUNC, "_cs_get_origin(%d)\n", origin);

	switch (origin) {
	case 1:
		return "external error";
		break;
	case 2:
		return "internal CS-Library error";
		break;
	case 4:
		return "common library error";
		break;
	case 5:
		return "intl library error";
		break;
	default:
		break;
	}
	return "unrecognized origin";
}

static const char *
_cs_get_user_api_layer_error(int error)
{
	tdsdump_log(TDS_DBG_FUNC, "_cs_get_user_api_layer_error(%d)\n", error);

	switch (error) {
	case 2:
		return "The information being retrieved will not fit in a buffer of %1! bytes.";
		break;
	case 3:
		return "Memory allocation failure.";
		break;
	case 4:
		return "The parameter %1! cannot be NULL.";
		break;
	case 6:
		return "An illegal value of %1! was given for parameter %2!.";
		break;
	case 16:
		return "Conversion between %1! and %2! datatypes is not supported.";
		break;
	case 18:
		return "An illegal value of %1! was placed in the %2! field of the CS_DATAFMT structure.";
		break;
	case 20:
		return "The conversion/operation resulted in overflow.";
		break;
	case 24:
		return "The conversion/operation was stopped due to a syntax error in the source field.";
		break;
	case 36:
		return "The result is truncated because the conversion/operation resulted in overflow.";
		break;
	default:
		break;
	}
	return "unrecognized error";
}

static char *
_cs_get_msgstr(const char *funcname, int layer, int origin, int severity, int number)
{
	char *m;

	tdsdump_log(TDS_DBG_FUNC, "_cs_get_msgstr(%s, %d, %d, %d, %d)\n", funcname, layer, origin, severity, number);

	if (asprintf(&m, "%s: %s: %s: %s", funcname, _cs_get_layer(layer), _cs_get_origin(origin), (layer == 2)
		     ? _cs_get_user_api_layer_error(number)
		     : "unrecognized error") < 0) {
		return NULL;
	}
	return m;
}

static void
_csclient_msg(CS_CONTEXT * ctx, const char *funcname, int layer, int origin, int severity, int number, const char *fmt, ...)
{
	va_list ap;
	CS_CLIENTMSG cm;
	char *msgstr;

	tdsdump_log(TDS_DBG_FUNC, "_csclient_msg(%p, %s, %d, %d, %d, %d, %s)\n", ctx, funcname, layer, origin, severity, number, fmt);

	va_start(ap, fmt);

	if (ctx->cslibmsg_cb) {
		cm.severity = severity;
		cm.msgnumber = (((layer << 24) & 0xFF000000)
				| ((origin << 16) & 0x00FF0000)
				| ((severity << 8) & 0x0000FF00)
				| ((number) & 0x000000FF));
		msgstr = _cs_get_msgstr(funcname, layer, origin, severity, number);
		tds_vstrbuild(cm.msgstring, CS_MAX_MSG, &(cm.msgstringlen), msgstr, CS_NULLTERM, fmt, CS_NULLTERM, ap);
		cm.msgstring[cm.msgstringlen] = '\0';
		free(msgstr);
		cm.osnumber = 0;
		cm.osstring[0] = '\0';
		cm.osstringlen = 0;
		cm.status = 0;
		/* cm.sqlstate */
		cm.sqlstatelen = 0;
		ctx->cslibmsg_cb(ctx, &cm);
	}

	va_end(ap);
}

/**
 * Allocate new CS_LOCALE and initialize it
 * returns NULL on out of memory errors 
 */
static CS_LOCALE *
_cs_locale_alloc(void)
{
	tdsdump_log(TDS_DBG_FUNC, "_cs_locale_alloc()\n");

	return tds_new0(CS_LOCALE, 1);
}

static void
_cs_locale_free_contents(CS_LOCALE *locale)
{
	tdsdump_log(TDS_DBG_FUNC, "_cs_locale_free_contents(%p)\n", locale);

	/* free strings */
	free(locale->language);
	locale->language = NULL;
	free(locale->charset);
	locale->charset = NULL;
	free(locale->time);
	locale->time = NULL;
	free(locale->collate);
	locale->collate = NULL;
}

void 
_cs_locale_free(CS_LOCALE *locale)
{
	tdsdump_log(TDS_DBG_FUNC, "_cs_locale_free(%p)\n", locale);

	/* free contents */
	_cs_locale_free_contents(locale);

	/* free the data structure */
	free(locale);
}

/* returns 0 on out of memory errors, 1 for OK */
int
_cs_locale_copy_inplace(CS_LOCALE *new_locale, CS_LOCALE *orig)
{
	tdsdump_log(TDS_DBG_FUNC, "_cs_locale_copy_inplace(%p, %p)\n", new_locale, orig);

	_cs_locale_free_contents(new_locale);
	if (orig->language) {
		new_locale->language = strdup(orig->language);
		if (!new_locale->language)
			goto Cleanup;
	}

	if (orig->charset) {
		new_locale->charset = strdup(orig->charset);
		if (!new_locale->charset)
			goto Cleanup;
	}

	if (orig->time) {
		new_locale->time = strdup(orig->time);
		if (!new_locale->time)
			goto Cleanup;
	}

	if (orig->collate) {
		new_locale->collate = strdup(orig->collate);
		if (!new_locale->collate)
			goto Cleanup;
	}

	return 1;

Cleanup:
	_cs_locale_free_contents(new_locale);
	return 0;
}

/* returns NULL on out of memory errors */
CS_LOCALE *
_cs_locale_copy(CS_LOCALE *orig)
{
	CS_LOCALE *new_locale;

	tdsdump_log(TDS_DBG_FUNC, "_cs_locale_copy(%p)\n", orig);

	new_locale = _cs_locale_alloc();
	if (!new_locale)
		return NULL;

	if (orig->language) {
		new_locale->language = strdup(orig->language);
		if (!new_locale->language)
			goto Cleanup;
	}

	if (orig->charset) {
		new_locale->charset = strdup(orig->charset);
		if (!new_locale->charset)
			goto Cleanup;
	}

	if (orig->time) {
		new_locale->time = strdup(orig->time);
		if (!new_locale->time)
			goto Cleanup;
	}

	if (orig->collate) {
		new_locale->collate = strdup(orig->collate);
		if (!new_locale->collate)
			goto Cleanup;
	}

	return new_locale;

Cleanup:
	_cs_locale_free(new_locale);
	return NULL;
}

CS_RETCODE
cs_ctx_alloc(CS_INT version, CS_CONTEXT ** out_ctx)
{
	TDSCONTEXT *tds_ctx;
	CS_CONTEXT *ctx;

	tdsdump_log(TDS_DBG_FUNC, "cs_ctx_alloc(%d, %p)\n", version, out_ctx);

	ctx = tds_new0(CS_CONTEXT, 1);
	if (!ctx)
		return CS_FAIL;
	ctx->use_large_identifiers = _ct_is_large_identifiers_version(version);
	tds_ctx = tds_alloc_context(ctx);
	if (!tds_ctx) {
		free(ctx);
		return CS_FAIL;
	}
	ctx->tds_ctx = tds_ctx;
	if (tds_ctx->locale && !tds_ctx->locale->datetime_fmt) {
		/* set default in case there's no locale file */
		tds_ctx->locale->datetime_fmt = strdup(STD_DATETIME_FMT);
	}

	ctx->login_timeout = -1;
	ctx->query_timeout = -1;

	*out_ctx = ctx;
	return CS_SUCCEED;
}

CS_RETCODE
cs_ctx_global(CS_INT version, CS_CONTEXT ** ctx)
{
	static CS_CONTEXT *global_cs_ctx = NULL;

	tdsdump_log(TDS_DBG_FUNC, "cs_ctx_global(%d, %p)\n", version, ctx);

	if (global_cs_ctx != NULL) {
		*ctx = global_cs_ctx;
		return CS_SUCCEED;
	}
	if (cs_ctx_alloc(version, ctx) != CS_SUCCEED) {
		return CS_FAIL;
	}
	global_cs_ctx = *ctx;
	return CS_SUCCEED;
}

CS_RETCODE
cs_ctx_drop(CS_CONTEXT * ctx)
{
	tdsdump_log(TDS_DBG_FUNC, "cs_ctx_drop(%p)\n", ctx);

	if (ctx) {
		_ct_diag_clearmsg(ctx, CS_ALLMSG_TYPE);
		free(ctx->userdata);
		if (ctx->tds_ctx)
			tds_free_context(ctx->tds_ctx);
		free(ctx);
	}
	return CS_SUCCEED;
}

CS_RETCODE
cs_config(CS_CONTEXT * ctx, CS_INT action, CS_INT property, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{
	CS_INT maxcp;	/* declared  for - CS_USERDATA changes - swapna*/

	tdsdump_log(TDS_DBG_FUNC, "cs_config(%p, %d, %d, %p, %d, %p)\n", ctx, action, property, buffer, buflen, outlen);

	if (action == CS_GET) {
		if (buffer == NULL) {
			return CS_SUCCEED;
		}
		switch (property) {
		case CS_MESSAGE_CB:
			*(CS_CSLIBMSG_FUNC*) buffer = ctx->cslibmsg_cb;
			return CS_SUCCEED;
		case CS_USERDATA:
			if (buflen < 0) {
				_csclient_msg(ctx, "cs_config", 2, 1, 1, 6, "%d, %s", buflen, "buflen");
				return CS_FAIL;
			}

			maxcp = ctx->userdata_len;
			if (outlen)
				*outlen = maxcp;

			if (buflen < maxcp) {
				_csclient_msg(ctx, "cs_config", 2, 1, 1, 2, "%d", buflen);
				return CS_FAIL;
			}
			memcpy(buffer, ctx->userdata, maxcp); 
			return CS_SUCCEED;

		default:
		case CS_EXTRA_INF:
		case CS_LOC_PROP:
		case CS_VERSION:
			_csclient_msg(ctx, "cs_config", 2, 1, 1, 6, "%d, %s", property, "property");
			return CS_FAIL;
			break;
		}
	}
	if (action == CS_SET) {
		switch (property) {
		case CS_MESSAGE_CB:
			if ( ctx->cs_errhandletype == _CS_ERRHAND_INLINE) {
				cs_diag_clearmsg(ctx, CS_UNUSED);
			}
			ctx->cslibmsg_cb = (CS_CSLIBMSG_FUNC) buffer;
			ctx->cs_errhandletype = _CS_ERRHAND_CB;
			return CS_SUCCEED;
		case CS_USERDATA:
			if (buflen < 0 && buflen != CS_NULLTERM) {
				_csclient_msg(ctx, "cs_config", 2, 1, 1, 6, "%d, %s", buflen, "buflen");
				return CS_FAIL;
			}

			maxcp = _ct_get_string_length(buffer, buflen);
			free(ctx->userdata);
			ctx->userdata = (void *) malloc(maxcp);
			if ( ctx->userdata == NULL) {
				return CS_FAIL;
			}
			ctx->userdata_len = maxcp;
	
			if (buffer) {
				memcpy(ctx->userdata, buffer, maxcp);
			} else {
				return CS_FAIL;
			}
			return CS_SUCCEED;

		default:
		case CS_EXTRA_INF:
		case CS_LOC_PROP:
		case CS_VERSION:
			_csclient_msg(ctx, "cs_config", 2, 1, 1, 6, "%d, %s", property, "property");
			return CS_FAIL;
			break;
		}
	}
	if (action == CS_CLEAR) {
		switch (property) {
		case CS_MESSAGE_CB:
			if ( ctx->cs_errhandletype == _CS_ERRHAND_INLINE) {
				cs_diag_clearmsg(ctx, CS_UNUSED);
			}
			ctx->cslibmsg_cb = NULL;
			ctx->cs_errhandletype = 0;
			return CS_SUCCEED;
		case CS_USERDATA:
			free(ctx->userdata);
			ctx->userdata = NULL;
			ctx->userdata_len = 0;
	
			return CS_SUCCEED;

		default:
		case CS_EXTRA_INF:
		case CS_LOC_PROP:
		case CS_VERSION:
			_csclient_msg(ctx, "cs_config", 2, 1, 1, 6, "%d, %s", property, "property");
			return CS_FAIL;
			break;
		}
	}

	_csclient_msg(ctx, "cs_config", 2, 1, 1, 6, "%d, %s", action, "action");
	return CS_FAIL;
}

CS_RETCODE
_cs_convert(CS_CONTEXT * ctx, const CS_DATAFMT_COMMON * srcfmt, CS_VOID * srcdata,
	    const CS_DATAFMT_COMMON * destfmt, CS_VOID * destdata, CS_INT * resultlen, TDS_SERVER_TYPE desttype)
{
	TDS_SERVER_TYPE src_type;
	int src_len, destlen, len;
	CONV_RESULT cres;
	unsigned char *dest;
	CS_RETCODE ret;
	CS_INT dummy, datatype;
	CS_VARCHAR *destvc = NULL;

	tdsdump_log(TDS_DBG_FUNC, "cs_convert(%p, %p, %p, %p, %p, %p, %d)\n",
		    ctx, srcfmt, srcdata, destfmt, destdata, resultlen, desttype);

	/* If destfmt is NULL we have a problem */
	if (destfmt == NULL) {
		_csclient_msg(ctx, "cs_convert", 2, 1, 1, 4, "destfmt");
		return CS_FAIL;
	}

	/* If destination is NULL we have a problem */
	if (destdata == NULL) {
		_csclient_msg(ctx, "cs_convert", 2, 1, 1, 4, "destdata");
		return CS_FAIL;

	}

	if (resultlen == NULL) 
		resultlen = &dummy;

	/* If source is indicated to be NULL, set dest to low values */
	if (srcdata == NULL) {
		/* TODO: implement cs_setnull */
		tdsdump_log(TDS_DBG_FUNC, "srcdata is null\n");
		memset(destdata, '\0', destfmt->maxlength);
		*resultlen = 0;
		return CS_SUCCEED;
	}

	datatype = srcfmt->datatype;
	src_type = _ct_get_server_type(NULL, datatype);
	if (src_type == TDS_INVALID_TYPE) {
		_csclient_msg(ctx, "cs_convert", 2, 1, 1, 16, "%d, %d", srcfmt->datatype, destfmt->datatype);
		return CS_FAIL;
	}
	src_len = srcfmt->maxlength;
	if (datatype == CS_VARCHAR_TYPE || datatype == CS_VARBINARY_TYPE) {
		CS_VARCHAR *vc = (CS_VARCHAR *) srcdata;
		src_len = vc->len;
		srcdata = vc->str;
	}

	datatype = destfmt->datatype;
	if (desttype == TDS_INVALID_TYPE) {
		desttype = _ct_get_server_type(NULL, datatype);
		if (desttype == TDS_INVALID_TYPE) {
			_csclient_msg(ctx, "cs_convert", 2, 1, 1, 16, "%d, %d",
				      srcfmt->datatype, destfmt->datatype);
			return CS_FAIL;
		}
	}
	destlen = destfmt->maxlength;
	if (datatype == CS_VARCHAR_TYPE || datatype == CS_VARBINARY_TYPE) {
		destvc = (CS_VARCHAR *) destdata;
		destlen  = sizeof(destvc->str);
		destdata = destvc->str;
	} else if (is_numeric_type(desttype)) {
		destlen = sizeof(TDS_NUMERIC);
	}

	tdsdump_log(TDS_DBG_FUNC, "converting type %d (%d bytes) to type = %d (%d bytes)\n",
		    src_type, src_len, desttype, destlen);

	if (!is_fixed_type(desttype) && destlen < 0) {
		_csclient_msg(ctx, "cs_convert", 2, 1, 1, 18, "%d, %s", destlen, "maxlength");
		return CS_FAIL;
	}

	dest = (unsigned char *) destdata;

	/* many times we are asked to convert a data type to itself */

	if (src_type == desttype) {
		int minlen = src_len < destlen? src_len : destlen;

		tdsdump_log(TDS_DBG_FUNC, "cs_convert() srctype == desttype\n");
		switch (desttype) {

		case SYBLONGBINARY:
		case SYBBINARY:
		case SYBVARBINARY:
		case SYBIMAGE:
			memcpy(dest, srcdata, minlen);
			*resultlen = minlen;

			if (src_len > destlen) {
				tdsdump_log(TDS_DBG_FUNC, "error: src_len > destlen\n");
				_csclient_msg(ctx, "cs_convert", 2, 4, 1, 36, "");
				ret = CS_FAIL;
			} else {
				switch (destfmt->format) {
				case CS_FMT_PADNULL:
					memset(dest + src_len, '\0', destlen - src_len);
					*resultlen = destlen;
					/* fall through */
				case CS_FMT_UNUSED:
					ret = CS_SUCCEED;
					break;
				default:
					ret = CS_FAIL;
					break;
				}
			}
			if (destvc) {
				destvc->len = minlen;
				*resultlen = sizeof(*destvc);
			}
			break;

		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
			tdsdump_log(TDS_DBG_FUNC, "cs_convert() desttype = character\n");

			memcpy(dest, srcdata, minlen);
			*resultlen = minlen;

			if (src_len > destlen) {
				tdsdump_log(TDS_DBG_FUNC, "error: src_len > destlen\n");
				_csclient_msg(ctx, "cs_convert", 2, 4, 1, 36, "");
				ret = CS_FAIL;
			} else {
				switch (destfmt->format) {
				case CS_FMT_NULLTERM:
					if (src_len == destlen) {
						*resultlen = src_len;
						tdsdump_log(TDS_DBG_FUNC, "error: no room for null terminator\n");
						ret = CS_FAIL;
					} else {
						dest[src_len] = '\0';
						*resultlen = src_len + 1;
						ret = CS_SUCCEED;
					}
					break;

				case CS_FMT_PADBLANK:
					memset(dest + src_len, ' ', destlen - src_len);
					*resultlen = destlen;
					ret = CS_SUCCEED;
					break;
				case CS_FMT_PADNULL:
					memset(dest + src_len, '\0', destlen - src_len);
					*resultlen = destlen;
					ret = CS_SUCCEED;
					break;
				case CS_FMT_UNUSED:
					ret = CS_SUCCEED;
					break;
				default:
					tdsdump_log(TDS_DBG_FUNC, "no destination format specified!\n");
					ret = CS_FAIL;
					break;
				}
			}
			if (destvc) {
				destvc->len = minlen;
				*resultlen = sizeof(*destvc);
			}
			break;
		case SYBINT1:
		case SYBUINT1:
		case SYBINT2:
		case SYBUINT2:
		case SYBINT4:
		case SYBUINT4:
		case SYBINT8:
		case SYBUINT8:
		case SYBFLT8:
		case SYBREAL:
		case SYBBIT:
		case SYBMONEY:
		case SYBMONEY4:
		case SYBDATETIME:
		case SYBDATETIME4:
		case SYBTIME:
		case SYBDATE:
		case SYB5BIGDATETIME:
		case SYB5BIGTIME:
			*resultlen = tds_get_size_by_type(src_type);
			if (*resultlen > 0)
				memcpy(dest, srcdata, *resultlen);
			ret = CS_SUCCEED;
			break;

		case SYBNUMERIC:
		case SYBDECIMAL:
			src_len = tds_numeric_bytes_per_prec[((TDS_NUMERIC *) srcdata)->precision] + 2;
		case SYBBITN:
		case SYBUNIQUE:
			memcpy(dest, srcdata, minlen);
			*resultlen = minlen;

			if (src_len > destlen) {
				tdsdump_log(TDS_DBG_FUNC, "error: src_len > destlen\n");
				_csclient_msg(ctx, "cs_convert", 2, 4, 1, 36, "");
				ret = CS_FAIL;
			} else {
				ret = CS_SUCCEED;
			}
			break;

		default:
			tdsdump_log(TDS_DBG_FUNC, "error: unrecognized type\n");
			ret = CS_FAIL;
			break;
		}

		tdsdump_log(TDS_DBG_FUNC, "cs_convert() returning  %s\n", cs_prretcode(ret));
		return ret;

	}

	assert(src_type != desttype);
	
	/* set the output precision/scale for conversions to numeric type */
	if (is_numeric_type(desttype)) {
		cres.n.precision = destfmt->precision;
		cres.n.scale = destfmt->scale;
		if (destfmt->precision == CS_SRC_VALUE)
			cres.n.precision = srcfmt->precision;
		if (destfmt->scale == CS_SRC_VALUE)
			cres.n.scale = srcfmt->scale;
	}

	tdsdump_log(TDS_DBG_FUNC, "cs_convert() calling tds_convert\n");
	len = tds_convert(ctx->tds_ctx, src_type, srcdata, src_len, desttype, &cres);

	tdsdump_log(TDS_DBG_FUNC, "cs_convert() tds_convert returned %d\n", len);

	switch (len) {
	case TDS_CONVERT_NOAVAIL:
		_csclient_msg(ctx, "cs_convert", 2, 1, 1, 16, "%d, %d", src_type, desttype);
		return CS_FAIL;
		break;
	case TDS_CONVERT_SYNTAX:
		_csclient_msg(ctx, "cs_convert", 2, 4, 1, 24, "");
		return CS_FAIL;
		break;
	case TDS_CONVERT_NOMEM:
		_csclient_msg(ctx, "cs_convert", 2, 4, 1, 3, "");
		return CS_FAIL;
		break;
	case TDS_CONVERT_OVERFLOW:
		_csclient_msg(ctx, "cs_convert", 2, 4, 1, 20, "");
		return CS_FAIL;
		break;
	case TDS_CONVERT_FAIL:
		return CS_FAIL;
		break;
	default:
		if (len < 0) {
			return CS_FAIL;
		}
		break;
	}

	switch (desttype) {
	case SYBBINARY:
	case SYBVARBINARY:
	case SYBIMAGE:
		ret = CS_SUCCEED;
		if (len > destlen) {
			tdsdump_log(TDS_DBG_FUNC, "error_handler: Data-conversion resulted in overflow\n");
			_csclient_msg(ctx, "cs_convert", 2, 4, 1, 36, "");
			ret = CS_FAIL;
			len = destlen;
		}
		memcpy(dest, cres.ib, len);
		free(cres.ib);
		*resultlen = len;
		if (destvc) {
			destvc->len = len;
			*resultlen = sizeof(*destvc);
		} else if (destfmt->format == CS_FMT_PADNULL) {
			*resultlen = destlen;
			memset(dest + len, '\0', destlen - len);
		}
		break;
	case SYBBIT:
	case SYBBITN:
		/* fall trough, act same way of TINYINT */
	case SYBINT1:
	case SYBUINT1:
	case SYBINT2:
	case SYBUINT2:
	case SYBINT4:
	case SYBUINT4:
	case SYBINT8:
	case SYBUINT8:
	case SYBFLT8:
	case SYBREAL:
	case SYBMONEY:
	case SYBMONEY4:
	case SYBDATETIME:
	case SYBDATETIME4:
	case SYBTIME:
	case SYBDATE:
	case SYBUNIQUE:
	case SYB5BIGDATETIME:
	case SYB5BIGTIME:
		*resultlen = tds_get_size_by_type(desttype);
		memcpy(dest, &(cres.ti), *resultlen);
		ret = CS_SUCCEED;
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		src_len = tds_numeric_bytes_per_prec[cres.n.precision] + 2;
		memcpy(dest, &(cres.n), src_len);
		*resultlen = src_len;
		ret = CS_SUCCEED;
		break;
	case SYBMSTIME:
	case SYBMSDATE:
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
		*resultlen = sizeof(TDS_DATETIMEALL);
		memcpy(dest, &(cres.dta), *resultlen);
		ret = CS_SUCCEED;
		break;
	case SYBCHAR:
	case SYBVARCHAR:
	case SYBTEXT:
		ret = CS_SUCCEED;
		if (len > destlen) {
			tdsdump_log(TDS_DBG_FUNC, "Data-conversion resulted in overflow\n");
			_csclient_msg(ctx, "cs_convert", 2, 4, 1, 36, "");
			len = destlen;
			ret = CS_FAIL;
		}
		switch (destfmt->format) {

		case CS_FMT_NULLTERM:
			tdsdump_log(TDS_DBG_FUNC, "cs_convert() FMT_NULLTERM\n");
			if (len == destlen) {
				tdsdump_log(TDS_DBG_FUNC, "not enough room for data + a null terminator - error\n");
				ret = CS_FAIL;	/* not enough room for data + a null terminator - error */
			} else {
				memcpy(dest, cres.c, len);
				dest[len] = 0;
				*resultlen = len + 1;
			}
			break;

		case CS_FMT_PADBLANK:
			tdsdump_log(TDS_DBG_FUNC, "cs_convert() FMT_PADBLANK\n");
			/* strcpy here can lead to a small buffer overflow */
			memcpy(dest, cres.c, len);
			memset(dest + len, ' ', destlen - len);
			*resultlen = destlen;
			break;

		case CS_FMT_PADNULL:
			tdsdump_log(TDS_DBG_FUNC, "cs_convert() FMT_PADNULL\n");
			/* strcpy here can lead to a small buffer overflow */
			memcpy(dest, cres.c, len);
			memset(dest + len, '\0', destlen - len);
			*resultlen = destlen;
			break;
		case CS_FMT_UNUSED:
			tdsdump_log(TDS_DBG_FUNC, "cs_convert() FMT_UNUSED\n");
			memcpy(dest, cres.c, len);
			*resultlen = len;
			break;
		default:
			ret = CS_FAIL;
			break;
		}
		if (destvc) {
			destvc->len = len;
			*resultlen = sizeof(*destvc);
		}
		free(cres.c);
		break;
	default:
		ret = CS_FAIL;
		break;
	}
	tdsdump_log(TDS_DBG_FUNC, "cs_convert() returning  %s\n", cs_prretcode(ret));
	return (ret);
}

CS_RETCODE
cs_convert(CS_CONTEXT * ctx, CS_DATAFMT * srcfmt, CS_VOID * srcdata, CS_DATAFMT * destfmt, CS_VOID * destdata, CS_INT * resultlen)
{
	return _cs_convert(ctx, _ct_datafmt_common(ctx, srcfmt), srcdata,
			   _ct_datafmt_common(ctx, destfmt), destdata, resultlen, TDS_INVALID_TYPE);
}

CS_RETCODE
cs_dt_crack_v2(CS_CONTEXT * ctx, CS_INT datetype, CS_VOID * dateval, CS_DATEREC * daterec)
{
	TDSDATEREC dr;
	TDS_INT tds_type;
	bool extended = false;

	tdsdump_log(TDS_DBG_FUNC, "cs_dt_crack_v2(%p, %d, %p, %p)\n", ctx, datetype, dateval, daterec);

	switch (datetype) {
	case CS_DATETIME_TYPE:
		tds_type = SYBDATETIME;
		break;
	case CS_DATETIME4_TYPE:
		tds_type = SYBDATETIME4;
		break;
	case CS_DATE_TYPE:
		tds_type = SYBDATE;
		break;
	case CS_TIME_TYPE:
		tds_type = SYBTIME;
		break;
	case CS_BIGDATETIME_TYPE:
		tds_type = SYB5BIGDATETIME;
		extended = true;
		break;
	case CS_BIGTIME_TYPE:
		tds_type = SYB5BIGTIME;
		extended = true;
		break;
	default:
		return CS_FAIL;
	}
	tds_datecrack(tds_type, dateval, &dr);

	/* Sybase CT-Library does not set these fields for CS_BIGTIME_TYPE */
	if (tds_type != SYB5BIGTIME) {
		daterec->dateyear = dr.year;
		daterec->datemonth = dr.month;
		daterec->datedmonth = dr.day;
		daterec->datedyear = dr.dayofyear;
		daterec->datedweek = dr.weekday;
	}
	daterec->datehour = dr.hour;
	daterec->dateminute = dr.minute;
	daterec->datesecond = dr.second;
	daterec->datemsecond = dr.decimicrosecond / 10000u;
	daterec->datetzone = dr.timezone;
	if (extended) {
		daterec->datesecfrac = dr.decimicrosecond / 10u;
		daterec->datesecprec = 1000000;
	}

	return CS_SUCCEED;
}

CS_RETCODE
cs_dt_crack(CS_CONTEXT * ctx, CS_INT datetype, CS_VOID * dateval, CS_DATEREC * daterec)
{
	tdsdump_log(TDS_DBG_FUNC, "cs_dt_crack(%p, %d, %p, %p)\n", ctx, datetype, dateval, daterec);

	/* Avoid handling CS_BIGDATETIME_TYPE and CS_BIGTIME_TYPE as these
	 * types requires a larger structure that would cause a buffer overflow. */
	if (datetype != CS_BIGDATETIME_TYPE && datetype != CS_BIGTIME_TYPE)
		return cs_dt_crack_v2(ctx, datetype, dateval, daterec);
	return CS_FAIL;
}

CS_RETCODE
cs_loc_alloc(CS_CONTEXT * ctx, CS_LOCALE ** loc_pointer)
{
	CS_LOCALE *tds_csloc;

	tdsdump_log(TDS_DBG_FUNC, "cs_loc_alloc(%p, %p)\n", ctx, loc_pointer);

	if (!ctx)
		return CS_FAIL;

	if (!loc_pointer) {
		_csclient_msg(ctx, "cs_loc_alloc", 2, 1, 1, 4, "loc_pointer");
		return CS_FAIL;
	}

	tds_csloc = _cs_locale_alloc();
	if (!tds_csloc)
		return CS_FAIL;

	*loc_pointer = tds_csloc;
	return CS_SUCCEED;
}

CS_RETCODE
cs_loc_drop(CS_CONTEXT * ctx, CS_LOCALE * locale)
{
	tdsdump_log(TDS_DBG_FUNC, "cs_loc_drop(%p, %p)\n", ctx, locale);

	if (!ctx)
		return CS_FAIL;

	if (!locale) {
		_csclient_msg(ctx, "cs_loc_drop", 2, 1, 1, 4, "locale");
		return CS_FAIL;
	}

	_cs_locale_free(locale);
	return CS_SUCCEED;
}

CS_RETCODE
cs_locale(CS_CONTEXT * ctx, CS_INT action, CS_LOCALE * locale, CS_INT type, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{
	CS_RETCODE code = CS_FAIL;

	tdsdump_log(TDS_DBG_FUNC, "cs_locale(%p, %d, %p, %d, %p, %d, %p)\n", ctx, action, locale, type, buffer, buflen, outlen);

	if (!ctx)
		return CS_FAIL;

	if (!locale) {
		_csclient_msg(ctx, "cs_locale", 2, 1, 1, 4, "locale");
		return CS_FAIL;
	}

	if (action == CS_SET) {
		switch (type) {
		case CS_LC_ALL:
			/* what to do here if there is locale data? */
			if (!buffer) {
				code = CS_SUCCEED;
			}
			break;

		case CS_SYB_CHARSET:
			buflen = _ct_get_string_length(buffer, buflen);
			if (buflen < 0) {
				_csclient_msg(ctx, "cs_locale", 2, 1, 1, 6, "%d, buflen", buflen);
				return CS_FAIL;
			}
			
			free(locale->charset);
			locale->charset = tds_strndup(buffer, buflen);
			if (!locale->charset)
				break;

			code = CS_SUCCEED;
			break;

		case CS_SYB_LANG:
			buflen = _ct_get_string_length(buffer, buflen);
			if (buflen < 0) {
				_csclient_msg(ctx, "cs_locale", 2, 1, 1, 6, "%d, buflen", buflen);
				return CS_FAIL;
			}
			
			free(locale->language);
			locale->language = tds_strndup(buffer, buflen);
			if (!locale->language)
				break;

			code = CS_SUCCEED;
			break;

		case CS_SYB_LANG_CHARSET:
		{
			int i;
			char *b = (char *)buffer;

			buflen = _ct_get_string_length(buffer, buflen);
			if (buflen < 0) {
				_csclient_msg(ctx, "cs_locale", 2, 1, 1, 6, "%d, buflen", buflen);
				return CS_FAIL;
			}

			/* find '.' character */
			for (i = 0; i < buflen; ++i) {
				if (b[i] == '.') {
					break;
				}					
			}
			/* not found */
			if (i == buflen) {
				break;
			}
			if (i) {
				free(locale->language);
				locale->language = tds_strndup(b, i);
				if (!locale->language)
					break;
			}
			if (i != (buflen - 1)) {
				free(locale->charset);
				locale->charset = tds_strndup(b + i + 1, buflen - i - 1);
				if (!locale->charset)
					break;
			}
			code = CS_SUCCEED;
			break;
		}

		/* TODO commented out until the code works end-to-end
		case CS_SYB_SORTORDER:
			buflen = _ct_get_string_length(buffer, buflen);
			if (buflen < 0) {
				_csclient_msg(ctx, "cs_locale", 2, 1, 1, 6, "%d, buflen", buflen);
				return CS_FAIL;
			}
			
			free(locale->collate);
			locale->collate = tds_strndup(buffer, buflen);
			if (!locale->collate)
				break;

			code = CS_SUCCEED;
			break;
		*/

		default:
			_csclient_msg(ctx, "cs_locale", 2, 1, 1, 6, "%d, type", type);
			code = CS_FAIL;
			break;
		}
	}
	else if (action == CS_GET)
	{
		CS_INT tlen;

		switch (type) {
		case CS_SYB_CHARSET:
			tlen = (locale->charset ? (CS_INT) strlen(locale->charset) : 0) + 1;
			if (buflen < tlen)
			{
				if (outlen)
					*outlen = tlen;
				break;
			}
			if (locale->charset)
				strcpy((char *)buffer, locale->charset);
			else
				((char *)buffer)[0] = '\0';
			code = CS_SUCCEED;
			break;

		case CS_SYB_LANG:
			tlen = (locale->language ? (CS_INT) strlen(locale->language) : 0) + 1;
			if (buflen < tlen)
			{
				if (outlen)
					*outlen = tlen;
				break;
			}
			if (locale->language)
				strcpy((char *)buffer, locale->language);
			else
				((char *)buffer)[0] = '\0';
			code = CS_SUCCEED;
			break;

		case CS_SYB_LANG_CHARSET:
		{
			CS_INT clen;

			tlen = (locale->language ? (CS_INT) strlen(locale->language) : 0) + 1;
			clen = (locale->charset ? (CS_INT) strlen(locale->charset) : 0) + 1;
			
			if (buflen < (tlen + clen))
			{
				if (outlen)
					*outlen = tlen + clen;
				break;
			}
			if (locale->language)
				strcpy((char *)buffer, locale->language);
			else
				((char *)buffer)[0] = '\0';
			strcat((char *)buffer, ".");
			if (locale->charset) {
				tlen = (CS_INT) strlen((char *)buffer);
				strcpy((char *)buffer + tlen, locale->charset);
			}
			code = CS_SUCCEED;
			break;
		}

		case CS_SYB_SORTORDER:
			tlen = (locale->collate ? (CS_INT) strlen(locale->collate) : 0) + 1;
			if (buflen < tlen)
			{
				if (outlen)
					*outlen = tlen;
				break;
			}
			if (locale->collate)
				strcpy((char *)buffer, locale->collate);
			else
				((char *)buffer)[0] = '\0';
			code = CS_SUCCEED;
			break;

		default:
			_csclient_msg(ctx, "cs_locale", 2, 1, 1, 6, "%d, type", type);
			code = CS_FAIL;
			break;
		}
	} else {
		_csclient_msg(ctx, "cs_locale", 2, 1, 1, 6, "%d, action", action);
		code = CS_FAIL;
	}
	return code;
}

CS_RETCODE
cs_dt_info(CS_CONTEXT * ctx, CS_INT action, CS_LOCALE * locale, CS_INT type, CS_INT item, CS_VOID * buffer, CS_INT buflen,
	   CS_INT * outlen)
{
	tdsdump_log(TDS_DBG_FUNC, "cs_dt_info(%p, %d, %p, %d, %d, %p, %d, %p)\n", 
				ctx, action, locale, type, item, buffer, buflen, outlen);

	if (action == CS_SET) {
		switch (type) {
		case CS_DT_CONVFMT:
			break;
		}
	}
	return CS_SUCCEED;
}

CS_RETCODE
cs_strbuild(CS_CONTEXT * ctx, CS_CHAR * buffer, CS_INT buflen, CS_INT * resultlen, CS_CHAR * text, CS_INT textlen,
	    CS_CHAR * formats, CS_INT formatlen, ...)
{
	va_list ap;
	TDSRET rc;

	tdsdump_log(TDS_DBG_FUNC, "cs_strbuild(%p, %p, %d, %p, %p, %d, %p, %d)\n", 
				ctx, buffer, buflen, resultlen, text, textlen, formats, formatlen);

	va_start(ap, formatlen);
	rc = tds_vstrbuild(buffer, buflen, resultlen, text, textlen, formats, formatlen, ap);
	va_end(ap);

	return TDS_SUCCEED(rc) ? CS_SUCCEED : CS_FAIL;
}

CS_RETCODE
cs_calc(CS_CONTEXT * ctx, CS_INT op, CS_INT datatype, CS_VOID * var1, CS_VOID * var2, CS_VOID * dest)
{

	tdsdump_log(TDS_DBG_FUNC, "cs_calc(%p, %d, %d, %p, %p, %p)\n", ctx, op, datatype, var1, var2, dest);

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED cs_calc()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_cmp(CS_CONTEXT * ctx, CS_INT datatype, CS_VOID * var1, CS_VOID * var2, CS_INT * result)
{

	tdsdump_log(TDS_DBG_FUNC, "cs_cmp(%p, %d, %p, %p, %p)\n", ctx, datatype, var1, var2, result);

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED cs_cmp()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_conv_mult(CS_CONTEXT * ctx, CS_LOCALE * srcloc, CS_LOCALE * destloc, CS_INT * conv_multiplier)
{

	tdsdump_log(TDS_DBG_FUNC, "cs_conv_mult(%p, %p, %p, %p)\n", ctx, srcloc, destloc, conv_multiplier);

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED cs_conv_mult()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_diag(CS_CONTEXT * ctx, CS_INT operation, CS_INT type, CS_INT idx, CS_VOID * buffer)
{
	tdsdump_log(TDS_DBG_FUNC, "cs_diag(%p, %d, %d, %d, %p)\n", ctx, operation, type, idx, buffer);

	switch (operation) {

		case CS_INIT:
			if ( ctx->cs_errhandletype == _CS_ERRHAND_CB) {
				/* contrary to the manual page you don't seem to */
				/* be able to turn on inline message handling    */
				/* using cs_diag, once a callback is installed!  */
				return CS_FAIL;
			}
			ctx->cs_errhandletype = _CS_ERRHAND_INLINE;
			ctx->cs_diag_msglimit = CS_NO_LIMIT;
			ctx->cslibmsg_cb = (CS_CSLIBMSG_FUNC) cs_diag_storemsg;
			break;
		case CS_MSGLIMIT:
			if ( ctx->cs_errhandletype != _CS_ERRHAND_INLINE) {
				return CS_FAIL;
			}
			ctx->cs_diag_msglimit = *(CS_INT *)buffer;
			break;
		case CS_CLEAR:
			if ( ctx->cs_errhandletype != _CS_ERRHAND_INLINE) {
				return CS_FAIL;
			}
			return (cs_diag_clearmsg(ctx, type));

			break;
		case CS_GET:
			if ( ctx->cs_errhandletype != _CS_ERRHAND_INLINE) {
				return CS_FAIL;
			}
			if (buffer == NULL)
				return CS_FAIL;

			if (idx == 0 || (ctx->cs_diag_msglimit != CS_NO_LIMIT && idx > ctx->cs_diag_msglimit) )
				return CS_FAIL;

			return (cs_diag_getmsg(ctx, idx, (CS_CLIENTMSG *)buffer)); 
			
			break;
		case CS_STATUS:
			if ( ctx->cs_errhandletype != _CS_ERRHAND_INLINE) {
				return CS_FAIL;
			}
			if (buffer == NULL) 
				return CS_FAIL;

			return (cs_diag_countmsg(ctx, (CS_INT *)buffer));
			break;
	}
	return CS_SUCCEED;
		
}

CS_RETCODE
cs_manage_convert(CS_CONTEXT * ctx, CS_INT action, CS_INT srctype, CS_CHAR * srcname, CS_INT srcnamelen, CS_INT desttype,
		  CS_CHAR * destname, CS_INT destnamelen, CS_INT * conv_multiplier, CS_CONV_FUNC * func)
{

	tdsdump_log(TDS_DBG_FUNC, "cs_manage_convert(%p, %d, %d, %p, %d, %d, %p, %d, %p, %p)\n", 
				ctx, action, srctype, srcname, srcnamelen, desttype, destname, destnamelen, conv_multiplier, func);

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED cs_manage_convert()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_objects(CS_CONTEXT * ctx, CS_INT action, CS_OBJNAME * objname, CS_OBJDATA * objdata)
{

	tdsdump_log(TDS_DBG_FUNC, "cs_objects(%p, %d, %p, %p)\n", ctx, action, objname, objdata);

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED cs_objects()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_set_convert(CS_CONTEXT * ctx, CS_INT action, CS_INT srctype, CS_INT desttype, CS_CONV_FUNC * func)
{

	tdsdump_log(TDS_DBG_FUNC, "cs_set_convert(%p, %d, %d, %d, %p)\n", ctx, action, srctype, desttype, func);

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED cs_set_convert()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_setnull(CS_CONTEXT * ctx, CS_DATAFMT * datafmt, CS_VOID * buffer, CS_INT buflen)
{

	tdsdump_log(TDS_DBG_FUNC, "cs_setnull(%p, %p, %p, %d)\n", ctx, datafmt, buffer, buflen);

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED cs_setnull()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_strcmp(CS_CONTEXT * ctx, CS_LOCALE * locale, CS_INT type, CS_CHAR * str1, CS_INT len1, CS_CHAR * str2, CS_INT len2,
	  CS_INT * result)
{

	tdsdump_log(TDS_DBG_FUNC, "cs_strcmp(%p, %p, %d, %p, %d, %p, %d, %p)\n", 
					ctx, locale, type, str1, len1, str2, len2, result);

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED cs_strcmp()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_time(CS_CONTEXT * ctx, CS_LOCALE * locale, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen, CS_DATEREC * daterec)
{

	tdsdump_log(TDS_DBG_FUNC, "cs_time(%p, %p, %p, %d, %p, %p)\n", ctx, locale, buffer, buflen, outlen, daterec);

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED cs_time()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_will_convert(CS_CONTEXT * ctx, CS_INT srctype, CS_INT desttype, CS_BOOL * result)
{

	tdsdump_log(TDS_DBG_FUNC, "cs_will_convert(%p, %d, %d, %p)\n", ctx, srctype, desttype, result);

	srctype = _ct_get_server_type(NULL, srctype);
	desttype = _ct_get_server_type(NULL, desttype);
	*result = (tds_willconvert(srctype, desttype) ? CS_TRUE : CS_FALSE);
	return CS_SUCCEED;
}

static CS_INT 
cs_diag_storemsg(CS_CONTEXT *context, CS_CLIENTMSG *message)
{
	struct cs_diag_msg **curptr;
	CS_INT msg_count = 0;

	tdsdump_log(TDS_DBG_FUNC, "cs_diag_storemsg(%p, %p)\n", context, message);

	curptr = &(context->msgstore);

	/* if we already have a list of messages, */
	/* go to the end of the list...           */

	while (*curptr != NULL) {
		msg_count++;
		curptr = &((*curptr)->next);
	}

	/* messages over and above the agreed limit */
	/* are simply discarded...                  */

	if (context->cs_diag_msglimit != CS_NO_LIMIT &&
		msg_count >= context->cs_diag_msglimit) {
		return CS_FAIL;
	}

	*curptr = tds_new(struct cs_diag_msg, 1);
	if (*curptr == NULL) { 
		return CS_FAIL;
	} else {
		(*curptr)->next = NULL;
		(*curptr)->msg  = tds_new(CS_CLIENTMSG, 1);
		if ((*curptr)->msg == NULL) {
			return CS_FAIL;
		} else {
			memcpy((*curptr)->msg, message, sizeof(CS_CLIENTMSG));
		}
	}

	return CS_SUCCEED;
}

static CS_INT 
cs_diag_getmsg(CS_CONTEXT *context, CS_INT idx, CS_CLIENTMSG *message)
{
	struct cs_diag_msg *curptr;
	CS_INT msg_count = 0, msg_found = 0;

	tdsdump_log(TDS_DBG_FUNC, "cs_diag_getmsg(%p, %d, %p)\n", context, idx, message);

	curptr = context->msgstore;

	/* if we already have a list of messages, */
	/* go to the end of the list...           */

	while (curptr != NULL) {
		msg_count++;
		if (msg_count == idx) {
			msg_found++;
			break;
		}
		curptr = curptr->next;
	}
	if (msg_found) {
		memcpy(message, curptr->msg, sizeof(CS_CLIENTMSG));
		return CS_SUCCEED;
	} else {
		return CS_NOMSG;
	}
}

static CS_INT 
cs_diag_clearmsg(CS_CONTEXT *context, CS_INT type)
{
	struct cs_diag_msg *curptr, *freeptr;

	tdsdump_log(TDS_DBG_FUNC, "cs_diag_clearmsg(%p, %d)\n", context, type);

	curptr = context->msgstore;
	context->msgstore = NULL;

	while (curptr != NULL ) {
        	freeptr = curptr;
		curptr = freeptr->next;
        	free(freeptr->msg);
        	free(freeptr);
	}
	return CS_SUCCEED;
}

static CS_INT 
cs_diag_countmsg(CS_CONTEXT *context, CS_INT *count)
{
	struct cs_diag_msg *curptr;
	CS_INT msg_count = 0;

	tdsdump_log(TDS_DBG_FUNC, "cs_diag_countmsg(%p, %p)\n", context, count);

	curptr = context->msgstore;

	while (curptr != NULL) {
		msg_count++;
		curptr = curptr->next;
	}
	*count = msg_count;
	return CS_SUCCEED;
}

/**
 * Try to convert to a type we can handle.
 * Used before calling _cs_convert to handle TDS types that do not have a
 * corresponding CS_xxx_TYPE.
 * @param ctx             Library context, used for conversion, can be NULL.
 * @param curcol          Column with data to convert.
 * @param convert_buffer  Buffer to store conversion, can be NULL.
 * @param p_src           Pointer to pointer to source data, can be NULL.
 * @return Converted type or CS_ILLEGAL_TYPE.
 */
int
_cs_convert_not_client(CS_CONTEXT *ctx, const TDSCOLUMN *curcol, CONV_RESULT *convert_buffer, unsigned char **p_src)
{
	int ct_type;
	TDS_SERVER_TYPE desttype, srctype = curcol->column_type;

	if (srctype == SYBVARIANT)
		srctype = ((const TDSVARIANT *) curcol->column_data)->type;

	switch (srctype) {
	case SYBMSDATE:
		desttype = SYBDATE;
		ct_type = CS_DATE_TYPE;
		break;
	case SYBMSTIME:
		desttype = SYB5BIGTIME;
		ct_type = CS_BIGTIME_TYPE;
		break;
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
		desttype = SYB5BIGDATETIME;
		ct_type = CS_BIGDATETIME_TYPE;
		break;
	default:
		return CS_ILLEGAL_TYPE;
	}

	if (convert_buffer) {
		int len = tds_convert(ctx->tds_ctx, srctype, *p_src,
				      curcol->column_cur_size, desttype, convert_buffer);
		if (len < 0)
			return CS_ILLEGAL_TYPE; /* TODO _csclient_msg ?? */
		*p_src = (unsigned char *) convert_buffer;
	}
	return ct_type;
}
