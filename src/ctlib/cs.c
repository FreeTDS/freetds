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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "cspublic.h"
#include "tdsconvert.h"
#include "ctlib.h"
#include "replacements.h"

static char software_version[] = "$Id: cs.c,v 1.45 2003-12-26 18:11:08 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int _cs_datatype_length(int dtype);
static CS_INT cs_diag_storemsg(CS_CONTEXT *context, CS_CLIENTMSG *message);
static CS_INT cs_diag_clearmsg(CS_CONTEXT *context, CS_INT type);
static CS_INT cs_diag_getmsg(CS_CONTEXT *context, CS_INT idx, CS_CLIENTMSG *message);
static CS_INT cs_diag_countmsg(CS_CONTEXT *context, CS_INT *count);

/* 	returns the fixed length of the specified data type, or 0 if not a 
	fixed length data type */
static int 
_cs_datatype_length(int dtype)
{
	switch (dtype) {
		case SYBINT1:
			return 1;
		case SYBINT2:
			return 2;
		case SYBINT4:
			return 4;
		case SYBINT8:
			return 8;
		case SYBFLT8:
			return 8;
		case SYBREAL:
			return 4;
		case SYBBIT:
			return 1;
		case SYBMONEY:
			return 8;
		case SYBMONEY4:
			return 4;
		case SYBDATETIME:
			return 8;
		case SYBDATETIME4:
			return 4;
		default:
			return 0;
	}
}

static const char *
_cs_get_layer(int layer)
{
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
	switch (error) {
	case 3:
		return "Memory allocation failure.";
		break;
	case 16:
		return "Conversion between %1! and %2! datatypes is not supported.";
		break;
	case 20:
		return "The conversion/operation resulted in overflow.";
		break;
	case 24:
		return "The conversion/operation was stopped due to a syntax error in the source field.";
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

	va_start(ap, fmt);

	if (ctx->_cslibmsg_cb) {
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
		ctx->_cslibmsg_cb(ctx, &cm);
	}

	va_end(ap);
}

CS_RETCODE
cs_ctx_alloc(CS_INT version, CS_CONTEXT ** ctx)
{
TDSCONTEXT *tds_ctx;

	*ctx = (CS_CONTEXT *) malloc(sizeof(CS_CONTEXT));
	memset(*ctx, '\0', sizeof(CS_CONTEXT));
	tds_ctx = tds_alloc_context();
	tds_ctx_set_parent(tds_ctx, *ctx);
	(*ctx)->tds_ctx = tds_ctx;
	if (tds_ctx->locale && !tds_ctx->locale->date_fmt) {
		/* set default in case there's no locale file */
		tds_ctx->locale->date_fmt = strdup("%b %e %Y %I:%M%p");
	}
	return CS_SUCCEED;
}

CS_RETCODE
cs_ctx_global(CS_INT version, CS_CONTEXT ** ctx)
{
static CS_CONTEXT *global_cs_ctx = NULL;

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
	if (ctx) {
		if (ctx->tds_ctx)
			tds_free_context(ctx->tds_ctx);
		free(ctx);
	}
	return CS_SUCCEED;
}

CS_RETCODE
cs_config(CS_CONTEXT * ctx, CS_INT action, CS_INT property, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{
/* declared  for - CS_USERDATA changes - swapna*/
CS_INT maxcp;

	if (action == CS_GET) {
		if (buffer == NULL) {
			return CS_SUCCEED;
		}
		switch (property) {
		case CS_MESSAGE_CB:
			*(void **) buffer = (void*) ctx->_cslibmsg_cb;
			return CS_SUCCEED;
		case CS_USERDATA:
			maxcp = ctx->userdata_len;
			if (outlen)
				*outlen = maxcp;
			if (maxcp > buflen)
				maxcp = buflen;

			memcpy(buffer, ctx->userdata, maxcp); 
			
			return CS_SUCCEED;
		case CS_EXTRA_INF:
		case CS_LOC_PROP:
		case CS_VERSION:
			return CS_FAIL;
			break;
		}
	}
	/* CS_SET */
	switch (property) {
	case CS_MESSAGE_CB:
		if ( ctx->cs_errhandletype == _CS_ERRHAND_INLINE) {
			cs_diag_clearmsg(ctx, CS_UNUSED);
		}
		ctx->_cslibmsg_cb = (CS_CSLIBMSG_FUNC) buffer;
		ctx->cs_errhandletype = _CS_ERRHAND_CB;
		return CS_SUCCEED;
	case CS_USERDATA:
	   	if (ctx->userdata) {
		     free(ctx->userdata);
		}

		if (buflen == CS_NULLTERM) {
			maxcp = strlen(buffer) + 1;
		} else {
			maxcp = buflen;
		}
		ctx->userdata = (void *) malloc(maxcp);
		ctx->userdata_len = maxcp;

		memcpy(ctx->userdata, buffer, maxcp);
		return CS_SUCCEED;

	case CS_EXTRA_INF:
	case CS_LOC_PROP:
	case CS_VERSION:
		return CS_FAIL;
		break;
	}
	return CS_FAIL;
}

CS_RETCODE
cs_convert(CS_CONTEXT * ctx, CS_DATAFMT * srcfmt, CS_VOID * srcdata, CS_DATAFMT * destfmt, CS_VOID * destdata, CS_INT * resultlen)
{

int src_type, src_len, desttype, destlen, len, i = 0;

CONV_RESULT cres;

unsigned char *dest;

CS_RETCODE ret;

	tdsdump_log(TDS_DBG_FUNC, "%L cs_convert()\n");

	src_type = _ct_get_server_type(srcfmt->datatype);
	src_len = srcfmt ? srcfmt->maxlength : 0;
	desttype = _ct_get_server_type(destfmt->datatype);
	destlen = destfmt ? destfmt->maxlength : 0;

	tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() srctype = %d (%d) desttype = %d (%d)\n",
		    src_type, src_len, desttype, destlen);

	if (!is_fixed_type(desttype) && (destlen <= 0))
		return CS_SUCCEED;

	dest = (unsigned char *) destdata;

	/* If source is indicated to be NULL, set dest to low values */

	if (srcdata == NULL) {

		tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() srcdata is null\n");
		memset(dest, '\0', destlen);
		if (resultlen != (CS_INT *) NULL)
			*resultlen = 0;
		return CS_SUCCEED;

	}

	/* many times we are asked to convert a data type to itself */

	if (src_type == desttype) {

		tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() srctype = desttype\n");
		switch (desttype) {

		case SYBLONGBINARY:
		case SYBBINARY:
		case SYBVARBINARY:
		case SYBIMAGE:
			if (src_len > destlen) {
				ret = CS_FAIL;
			} else {
				switch (destfmt->format) {
				case CS_FMT_PADNULL:
					memcpy(dest, srcdata, src_len);
					for (i = src_len; i < destlen; i++)
						dest[i] = '\0';
					if (resultlen != (CS_INT *) NULL)
						*resultlen = destlen;
					ret = CS_SUCCEED;
					break;
				case CS_FMT_UNUSED:
					memcpy(dest, srcdata, src_len);
					if (resultlen != (CS_INT *) NULL)
						*resultlen = src_len;
					ret = CS_SUCCEED;
					break;
				default:
					ret = CS_FAIL;
					break;

				}
			}
			break;

		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
			tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() desttype = character\n");
			if (src_len > destlen) {
				tdsdump_log(TDS_DBG_FUNC, "%L src_len > destlen\n");
				ret = CS_FAIL;
			} else {
				switch (destfmt->format) {

				case CS_FMT_NULLTERM:
					if (src_len == destlen) {
						ret = CS_FAIL;	/* not enough room for data + a null terminator - error */
					} else {
						memcpy(dest, srcdata, src_len);
						dest[src_len] = '\0';
						if (resultlen != (CS_INT *) NULL)
							*resultlen = src_len + 1;
						ret = CS_SUCCEED;
					}
					break;

				case CS_FMT_PADBLANK:
					memcpy(dest, srcdata, src_len);
					for (i = src_len; i < destlen; i++)
						dest[i] = ' ';
					if (resultlen != (CS_INT *) NULL)
						*resultlen = destlen;
					ret = CS_SUCCEED;
					break;

				case CS_FMT_PADNULL:
					memcpy(dest, srcdata, src_len);
					for (i = src_len; i < destlen; i++)
						dest[i] = '\0';
					if (resultlen != (CS_INT *) NULL)
						*resultlen = destlen;
					ret = CS_SUCCEED;
					break;
				case CS_FMT_UNUSED:
					memcpy(dest, srcdata, src_len);
					if (resultlen != (CS_INT *) NULL)
						*resultlen = src_len;
					ret = CS_SUCCEED;
					break;
				default:
					tdsdump_log(TDS_DBG_FUNC, "%L no destination format specified!\n");
					ret = CS_FAIL;
					break;
				}
			}
			break;
		case SYBINT1:
		case SYBINT2:
		case SYBINT4:
		case SYBINT8:
		case SYBFLT8:
		case SYBREAL:
		case SYBBIT:
		case SYBMONEY:
		case SYBMONEY4:
		case SYBDATETIME:
		case SYBDATETIME4:
			memcpy(dest, srcdata, _cs_datatype_length(src_type));
			if (resultlen != (CS_INT *) NULL)
				*resultlen = _cs_datatype_length(src_type);
			ret = CS_SUCCEED;
			break;

		case SYBBITN:
		case SYBNUMERIC:
		case SYBDECIMAL:
			if (src_len > destlen) {
				ret = CS_FAIL;
			} else {
				memcpy(dest, srcdata, src_len);
				if (resultlen != (CS_INT *) NULL)
					*resultlen = src_len;
				ret = CS_SUCCEED;
			}
			break;

		default:
			ret = CS_FAIL;
			break;
		}

		return ret;

	}


	/* src type != dest type */
	/* set the output precision/scale for conversions to numeric type */
	if (is_numeric_type(desttype)) {
		cres.n.precision = destfmt->precision;
		cres.n.scale = destfmt->scale;
		if (destfmt->precision == CS_SRC_VALUE)
			cres.n.precision = srcfmt->precision;
		if (destfmt->scale == CS_SRC_VALUE)
			cres.n.scale = srcfmt->scale;
	}

	tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() calling tds_convert\n");
	len = tds_convert(ctx->tds_ctx, src_type, (TDS_CHAR*) srcdata, src_len, desttype, &cres);

	tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() tds_convert returned %d\n", len);

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

		if (len > destlen) {
			free(cres.ib);
			fprintf(stderr, "error_handler: Data-conversion resulted in overflow.\n");
			ret = CS_FAIL;
		} else {
			memcpy(dest, cres.ib, len);
			free(cres.ib);
			for (i = len; i < destlen; i++)
				dest[i] = '\0';
			if (resultlen != (CS_INT *) NULL)
				*resultlen = destlen;
			ret = CS_SUCCEED;
		}
		break;
	case SYBBIT:
	case SYBBITN:
		/* fall trough, act same way of TINYINT */
	case SYBINT1:
		memcpy(dest, &(cres.ti), 1);
		if (resultlen != (CS_INT *) NULL)
			*resultlen = 1;
		ret = CS_SUCCEED;
		break;
	case SYBINT2:
		memcpy(dest, &(cres.si), 2);
		if (resultlen != (CS_INT *) NULL)
			*resultlen = 2;
		ret = CS_SUCCEED;
		break;
	case SYBINT4:
		memcpy(dest, &(cres.i), 4);
		if (resultlen != (CS_INT *) NULL)
			*resultlen = 4;
		ret = CS_SUCCEED;
		break;
	case SYBINT8:
		memcpy(dest, &(cres.bi), 8);
		if (resultlen != (CS_INT *) NULL)
			*resultlen = 8;
		ret = CS_SUCCEED;
		break;
	case SYBFLT8:
		memcpy(dest, &(cres.f), 8);
		if (resultlen != (CS_INT *) NULL)
			*resultlen = 8;
		ret = CS_SUCCEED;
		break;
	case SYBREAL:
		memcpy(dest, &(cres.r), 4);
		if (resultlen != (CS_INT *) NULL)
			*resultlen = 4;
		ret = CS_SUCCEED;
		break;
	case SYBMONEY:

		tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() copying %d bytes to src\n", sizeof(TDS_MONEY));
		memcpy(dest, &(cres.m), sizeof(TDS_MONEY));
		if (resultlen != (CS_INT *) NULL)
			*resultlen = sizeof(TDS_MONEY);
		ret = CS_SUCCEED;
		break;
	case SYBMONEY4:
		memcpy(dest, &(cres.m4), sizeof(TDS_MONEY4));
		if (resultlen != (CS_INT *) NULL)
			*resultlen = sizeof(TDS_MONEY4);
		ret = CS_SUCCEED;
		break;
	case SYBDATETIME:
		memcpy(dest, &(cres.dt), sizeof(TDS_DATETIME));
		if (resultlen != (CS_INT *) NULL)
			*resultlen = sizeof(TDS_DATETIME);
		ret = CS_SUCCEED;
		break;
	case SYBDATETIME4:
		memcpy(dest, &(cres.dt4), sizeof(TDS_DATETIME4));
		if (resultlen != (CS_INT *) NULL)
			*resultlen = sizeof(TDS_DATETIME4);
		ret = CS_SUCCEED;
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		memcpy(dest, &(cres.n), sizeof(TDS_NUMERIC));
		if (resultlen != (CS_INT *) NULL)
			*resultlen = sizeof(TDS_NUMERIC);
		ret = CS_SUCCEED;
		break;
	case SYBCHAR:
	case SYBVARCHAR:
	case SYBTEXT:
		if (len > destlen) {
			fprintf(stderr, "error_handler: Data-conversion resulted in overflow.\n");
			ret = CS_FAIL;
		} else {
			switch (destfmt->format) {

			case CS_FMT_NULLTERM:
				tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() FMT_NULLTERM\n");
				if (len == destlen) {
					tdsdump_log(TDS_DBG_FUNC, "%L not enough room for data + a null terminator - error\n");
					ret = CS_FAIL;	/* not enough room for data + a null terminator - error */
				} else {
					memcpy(dest, cres.c, len);
					dest[len] = 0;
					if (resultlen != (CS_INT *) NULL)
						*resultlen = len + 1;
					ret = CS_SUCCEED;
				}
				break;

			case CS_FMT_PADBLANK:
				tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() FMT_PADBLANK\n");
				/* strcpy here can lead to a small buffer overflow */
				memcpy(dest, cres.c, len);
				for (i = len; i < destlen; i++)
					dest[i] = ' ';
				if (resultlen != (CS_INT *) NULL)
					*resultlen = destlen;
				ret = CS_SUCCEED;
				break;

			case CS_FMT_PADNULL:
				tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() FMT_PADNULL\n");
				/* strcpy here can lead to a small buffer overflow */
				memcpy(dest, cres.c, len);
				for (i = len; i < destlen; i++)
					dest[i] = '\0';
				if (resultlen != (CS_INT *) NULL)
					*resultlen = destlen;
				ret = CS_SUCCEED;
				break;
			case CS_FMT_UNUSED:
				tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() FMT_UNUSED\n");
				memcpy(dest, cres.c, len);
				if (resultlen != (CS_INT *) NULL)
					*resultlen = len;
				ret = CS_SUCCEED;
				break;
			default:
				ret = CS_FAIL;
				break;
			}
		}
		free(cres.c);
		break;
	default:
		ret = CS_FAIL;
		break;
	}
	tdsdump_log(TDS_DBG_FUNC, "%L cs_convert() returning  %d\n", ret);
	return (ret);
}

CS_RETCODE
cs_dt_crack(CS_CONTEXT * ctx, CS_INT datetype, CS_VOID * dateval, CS_DATEREC * daterec)
{
CS_DATETIME *dt;
CS_DATETIME4 *dt4;
time_t tmp_secs_from_epoch;
struct tm *t;
TDSDATEREC dr;

	if (datetype == CS_DATETIME_TYPE) {
		dt = (TDS_DATETIME *) dateval;
		tds_datecrack(SYBDATETIME, dt, &dr);
	} else if (datetype == CS_DATETIME4_TYPE) {
		dt4 = (TDS_DATETIME4 *) dateval;
		tds_datecrack(SYBDATETIME4, dt4, &dr);
	} else {
		return CS_FAIL;
	}
	t = (struct tm *) gmtime(&tmp_secs_from_epoch);
	daterec->dateyear = dr.year;
	daterec->datemonth = dr.month;
	daterec->datedmonth = dr.day;
	daterec->datedyear = dr.dayofyear;
	daterec->datedweek = dr.weekday;
	daterec->datehour = dr.hour;
	daterec->dateminute = dr.minute;
	daterec->datesecond = dr.second;
	daterec->datetzone = 0;

	return CS_SUCCEED;
}

CS_RETCODE
cs_loc_alloc(CS_CONTEXT * ctx, CS_LOCALE ** locptr)
{
	return CS_SUCCEED;
}

CS_RETCODE
cs_loc_drop(CS_CONTEXT * ctx, CS_LOCALE * locale)
{
	return CS_SUCCEED;
}

CS_RETCODE
cs_locale(CS_CONTEXT * ctx, CS_INT action, CS_LOCALE * locale, CS_INT type, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{
	return CS_SUCCEED;
}

CS_RETCODE
cs_dt_info(CS_CONTEXT * ctx, CS_INT action, CS_LOCALE * locale, CS_INT type, CS_INT item, CS_VOID * buffer, CS_INT buflen,
	   CS_INT * outlen)
{
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
	CS_RETCODE rc;

	va_start(ap, formatlen);
	rc = tds_vstrbuild(buffer, buflen, resultlen, text, textlen, formats, formatlen, ap);
	va_end(ap);
	return rc;
}

CS_RETCODE
cs_calc(CS_CONTEXT * ctx, CS_INT op, CS_INT datatype, CS_VOID * var1, CS_VOID * var2, CS_VOID * dest)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED cs_calc()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_cmp(CS_CONTEXT * ctx, CS_INT datatype, CS_VOID * var1, CS_VOID * var2, CS_INT * result)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED cs_cmp()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_conv_mult(CS_CONTEXT * ctx, CS_LOCALE * srcloc, CS_LOCALE * destloc, CS_INT * conv_multiplier)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED cs_conv_mult()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_diag(CS_CONTEXT * ctx, CS_INT operation, CS_INT type, CS_INT idx, CS_VOID * buffer)
{
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
			ctx->_cslibmsg_cb = (CS_CSLIBMSG_FUNC) cs_diag_storemsg; 
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

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED cs_manage_convert()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_objects(CS_CONTEXT * ctx, CS_INT action, CS_OBJNAME * objname, CS_OBJDATA * objdata)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED cs_objects()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_set_convert(CS_CONTEXT * ctx, CS_INT action, CS_INT srctype, CS_INT desttype, CS_CONV_FUNC * func)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED cs_set_convert()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_setnull(CS_CONTEXT * ctx, CS_DATAFMT * datafmt, CS_VOID * buffer, CS_INT buflen)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED cs_setnull()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_strcmp(CS_CONTEXT * ctx, CS_LOCALE * locale, CS_INT type, CS_CHAR * str1, CS_INT len1, CS_CHAR * str2, CS_INT len2,
	  CS_INT * result)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED cs_strcmp()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_time(CS_CONTEXT * ctx, CS_LOCALE * locale, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen, CS_DATEREC * daterec)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED cs_time()\n");
	return CS_FAIL;
}

CS_RETCODE
cs_will_convert(CS_CONTEXT * ctx, CS_INT srctype, CS_INT desttype, CS_BOOL * result)
{

	*result = (tds_willconvert(srctype, desttype) ? CS_TRUE : CS_FALSE);
	return CS_SUCCEED;
}

static CS_INT 
cs_diag_storemsg(CS_CONTEXT *context, CS_CLIENTMSG *message)
{

struct cs_diag_msg **curptr;
CS_INT msg_count = 0;

	curptr = &(context->msgstore);

	/* if we already have a list of messages, */
	/* go to the end of the list...           */

	while (*curptr != (struct cs_diag_msg *)NULL) {
		msg_count++;
		curptr = &((*curptr)->next);
	}

	/* messages over and above the agreed limit */
	/* are simply discarded...                  */

	if (context->cs_diag_msglimit != CS_NO_LIMIT &&
		msg_count >= context->cs_diag_msglimit) {
		return CS_FAIL;
	}

	*curptr = (struct cs_diag_msg *) malloc(sizeof(struct cs_diag_msg));
	if (*curptr == (struct cs_diag_msg *)NULL) { 
		return CS_FAIL;
	} else {
		(*curptr)->next = (struct cs_diag_msg *)NULL;
		(*curptr)->msg  = malloc(sizeof(CS_CLIENTMSG));
		if ((*curptr)->msg == (CS_CLIENTMSG *) NULL) {
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

	curptr = context->msgstore;

	/* if we already have a list of messages, */
	/* go to the end of the list...           */

	while (curptr != (struct cs_diag_msg *)NULL) {
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

	curptr = context->msgstore;
	context->msgstore = NULL;

	while (curptr != (struct cs_diag_msg *)NULL ) {
        	freeptr = curptr;
		curptr = freeptr->next;
        	if (freeptr->msg)
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

	curptr = context->msgstore;

	while (curptr != (struct cs_diag_msg *)NULL) {
		msg_count++;
		curptr = curptr->next;
	}
	*count = msg_count;
	return CS_SUCCEED;
}
