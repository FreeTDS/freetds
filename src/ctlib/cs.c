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

#include <cspublic.h>
#include <tdsconvert.h>
#include <time.h>
#include <stdarg.h>
#include "ctlib.h"
#include "tdsutil.h"

static char  software_version[]   = "$Id: cs.c,v 1.22 2002-09-28 00:33:31 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

static char *
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

static char *
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

static char *
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
_cs_get_msgstr(char *funcname, int layer, int origin, int severity, int number)
{
char *m;

	if (asprintf(&m, "%s: %s: %s: %s",
		funcname, 
		_cs_get_layer(layer),
		_cs_get_origin(origin),
		(layer == 2)
			? _cs_get_user_api_layer_error(number)
			: "unrecognized error") < 0) {
		return NULL;
	}
	return m;
}

static void
_csclient_msg(CS_CONTEXT *ctx, char *funcname, int layer, int origin, int severity, int number, char *fmt, ...)
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
		tds_vstrbuild(cm.msgstring, CS_MAX_MSG, &(cm.msgstringlen),
			msgstr, CS_NULLTERM, fmt, CS_NULLTERM, ap);
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
cs_ctx_alloc(CS_INT version, CS_CONTEXT **ctx)
{
TDSCONTEXT *tds_ctx;

	*ctx = (CS_CONTEXT *) malloc(sizeof(CS_CONTEXT));
	memset(*ctx,'\0',sizeof(CS_CONTEXT));
	tds_ctx = tds_alloc_context();
	tds_ctx_set_parent(tds_ctx, *ctx);
	(*ctx)->tds_ctx = tds_ctx;
	if( tds_ctx->locale && !tds_ctx->locale->date_fmt ) {
		/* set default in case there's no locale file */
		tds_ctx->locale->date_fmt = strdup("%b %e %Y %l:%M%p"); 
	}
	return CS_SUCCEED;
}

CS_RETCODE
cs_ctx_global(CS_INT version, CS_CONTEXT **ctx)
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

CS_RETCODE cs_ctx_drop(CS_CONTEXT *ctx)
{
	if (ctx) {
		if (ctx->tds_ctx) 
			tds_free_context(ctx->tds_ctx);
		free(ctx);
	}
	return CS_SUCCEED;
}
CS_RETCODE
cs_config(CS_CONTEXT *ctx, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen)
{
	if (action == CS_GET) {
		if (buffer == NULL) {
			return CS_SUCCEED;
		}
		switch (property) {
		case CS_MESSAGE_CB:
			*(void **)buffer = ctx->_cslibmsg_cb;
			return CS_SUCCEED;
		case CS_EXTRA_INF:
		case CS_LOC_PROP:
		case CS_USERDATA:
		case CS_VERSION:
			return CS_FAIL;
			break;
		}
	}
	/* CS_SET */
	switch (property) {
		case CS_MESSAGE_CB:
			ctx->_cslibmsg_cb = (void *) buffer;
			return CS_SUCCEED;
		case CS_EXTRA_INF:
		case CS_LOC_PROP:
		case CS_USERDATA:
		case CS_VERSION:
			return CS_FAIL;
			break;
	}
	return CS_FAIL;
}

CS_RETCODE cs_convert(CS_CONTEXT *ctx, CS_DATAFMT *srcfmt, CS_VOID *srcdata, 
                      CS_DATAFMT *destfmt, CS_VOID *destdata, CS_INT *resultlen)
{

int src_type, src_len, desttype, destlen, len, i = 0;

CONV_RESULT cres;

unsigned char *dest;

CS_RETCODE ret;

    tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert()\n");

    src_type = _ct_get_server_type(srcfmt->datatype);
    src_len  =  srcfmt ? srcfmt->maxlength : 0;
    desttype = _ct_get_server_type(destfmt->datatype);
    destlen  =  destfmt ? destfmt->maxlength : 0;

    tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() srctype = %d (%d) desttype = %d (%d)\n",
                src_type, src_len, desttype, destlen);

    if (destlen <= 0)
       return CS_SUCCEED;

    dest = (unsigned char *)destdata;

    /* If source is indicated to be NULL, set dest to low values */

    if (srcdata == NULL) {

       tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() srcdata is null\n");
       memset(dest,'\0', destlen);
       if (resultlen != (CS_INT *)NULL ) *resultlen = 0;
       return CS_SUCCEED;

    }
       
    /* many times we are asked to convert a data type to itself */

    if (src_type == desttype) {

       tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() srctype = desttype\n");
       switch (desttype) {

          case SYBBINARY:
          case SYBIMAGE:
               if (src_len > destlen) {
                  ret = CS_FAIL;
               }
               else {
                  switch (destfmt->format) {
                      case CS_FMT_PADNULL:
                          memcpy(dest, srcdata, src_len);
                          for ( i = src_len; i < destlen; i++)
                              dest[i] = '\0';
                          if (resultlen != (CS_INT *)NULL ) *resultlen = destlen;
                          ret = CS_SUCCEED;
                          break;
                      case CS_FMT_UNUSED:
                          memcpy(dest, srcdata, src_len);
                          if (resultlen != (CS_INT *)NULL ) *resultlen = src_len;
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
               tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() desttype = character\n");
               if (src_len > destlen ) {
                  ret = CS_FAIL;
               }
               else {
                  switch (destfmt->format) {
      
                      case CS_FMT_NULLTERM:
                          if (src_len == destlen ) { 
                             ret = CS_FAIL;    /* not enough room for data + a null terminator - error */
                          }
                          else {
                             memcpy(dest, srcdata, src_len);
                             dest[src_len] = '\0';
                             if (resultlen != (CS_INT *)NULL ) *resultlen = src_len+1;
                             ret = CS_SUCCEED;
                          }
                          break;
                             
                      case CS_FMT_PADBLANK:
                          memcpy(dest, srcdata, src_len);
                          for ( i = src_len; i < destlen; i++)
                              dest[i] = ' ';
                          if (resultlen != (CS_INT *)NULL ) *resultlen = destlen;
                          ret = CS_SUCCEED;
                          break;

                      case CS_FMT_PADNULL:
                          memcpy(dest, srcdata, src_len);
                          for ( i = src_len; i < destlen; i++)
                              dest[i] = '\0';
                          if (resultlen != (CS_INT *)NULL ) *resultlen = destlen;
                          ret = CS_SUCCEED;
                          break;
                      case CS_FMT_UNUSED:
                          memcpy(dest, srcdata, src_len);
                          if (resultlen != (CS_INT *)NULL ) *resultlen = src_len;
                          ret = CS_SUCCEED;
                          break;
                      default:
                          ret = CS_FAIL;
                          break;
                  }
               }
               break;
          case SYBINT1:
          case SYBINT2:
          case SYBINT4:
          case SYBFLT8:
          case SYBREAL:
          case SYBBIT:
          case SYBBITN:
          case SYBMONEY:
          case SYBMONEY4:
          case SYBDATETIME:
          case SYBDATETIME4:
               if (src_len > destlen) {
                  ret = CS_FAIL;
               }
               else {
                  memcpy (dest, srcdata, src_len);
                  if (resultlen != (CS_INT *)NULL ) *resultlen = src_len;
                  ret = CS_SUCCEED;
               } 
               break;

          case SYBNUMERIC:
          case SYBDECIMAL:
               if (src_len > destlen ) {
                  ret = CS_FAIL;
               }
               else {
                  memcpy (dest, srcdata, src_len);
                  if (resultlen != (CS_INT *)NULL ) *resultlen = src_len;
                  ret = CS_SUCCEED;
               } 
               break;
 
          default:
               ret = CS_FAIL;
               break;
       }
      
       return ret;

    }  /* src type == dest type */


	/* set the output precision/scale for conversions to numeric type */
	if (is_numeric_type(desttype)) {
		cres.n.precision = destfmt->precision;
		cres.n.scale     = destfmt->scale;
		if (destfmt->precision == CS_SRC_VALUE)
			cres.n.precision = srcfmt->precision;
		if (destfmt->scale == CS_SRC_VALUE)
			cres.n.scale = srcfmt->scale;
	}
    
    tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() calling tds_convert\n");
    len = tds_convert(ctx->tds_ctx, src_type, srcdata, src_len, desttype, &cres);

    tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() tds_convert returned %d\n", len);

    switch (len) {
    case TDS_CONVERT_NOAVAIL:
        _csclient_msg(ctx, "cs_convert", 2, 1, 1, 16,
			"%d, %d", src_type, desttype);
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
        case SYBIMAGE:

             if (len > destlen) {
                free(cres.ib);
                fprintf(stderr,"error_handler: Data-conversion resulted in overflow.\n");
                ret = CS_FAIL;
             }
             else {
                memcpy(dest, cres.ib, len);
                free(cres.ib);
                for ( i = len ; i < destlen; i++ )
                    dest[i] = '\0';
                if (resultlen != (CS_INT *)NULL ) *resultlen = destlen;
                ret = CS_SUCCEED;
             }
             break;
        case SYBBIT:
        case SYBBITN:
	     /* fall trough, act same way of TINYINT */
        case SYBINT1:
             memcpy(dest,&(cres.ti),1);
             if (resultlen != (CS_INT *)NULL ) *resultlen = 1;
             ret = CS_SUCCEED;
             break;
        case SYBINT2:
             memcpy(dest,&(cres.si),2);
             if (resultlen != (CS_INT *)NULL ) *resultlen = 2;
             ret = CS_SUCCEED;
             break;
        case SYBINT4:
             memcpy(dest,&(cres.i),4);
             if (resultlen != (CS_INT *)NULL ) *resultlen = 4;
             ret = CS_SUCCEED;
             break;
        case SYBFLT8:
             memcpy(dest,&(cres.f),8);
             if (resultlen != (CS_INT *)NULL ) *resultlen = 8;
             ret = CS_SUCCEED;
             break;
        case SYBREAL:
             memcpy(dest,&(cres.r),4);
             if (resultlen != (CS_INT *)NULL ) *resultlen = 4;
             ret = CS_SUCCEED;
             break;
        case SYBMONEY:
             
             tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() copying %d bytes to src\n", sizeof(TDS_MONEY));
             memcpy(dest,&(cres.m),sizeof(TDS_MONEY));
             if (resultlen != (CS_INT *)NULL ) *resultlen = sizeof(TDS_MONEY);
             ret = CS_SUCCEED;
             break;
        case SYBMONEY4:
             memcpy(dest,&(cres.m4),sizeof(TDS_MONEY4));
             if (resultlen != (CS_INT *)NULL ) *resultlen = sizeof(TDS_MONEY4);
             ret = CS_SUCCEED;
             break;
        case SYBDATETIME:
             memcpy(dest,&(cres.dt),sizeof(TDS_DATETIME));
             if (resultlen != (CS_INT *)NULL ) *resultlen = sizeof(TDS_DATETIME);
             ret = CS_SUCCEED;
             break;
        case SYBDATETIME4:
             memcpy(dest,&(cres.dt4),sizeof(TDS_DATETIME4));
             if (resultlen != (CS_INT *)NULL ) *resultlen = sizeof(TDS_DATETIME4);
             ret = CS_SUCCEED;
             break;
        case SYBNUMERIC:
        case SYBDECIMAL:
             memcpy(dest,&(cres.n), sizeof(TDS_NUMERIC));
             if (resultlen != (CS_INT *)NULL ) *resultlen = sizeof(TDS_NUMERIC);
             ret = CS_SUCCEED;
             break;
        case SYBCHAR:
        case SYBVARCHAR:
        case SYBTEXT:
             if (len > destlen) {
                fprintf(stderr,"error_handler: Data-conversion resulted in overflow.\n");
                ret = CS_FAIL;
             }
             else {
                switch (destfmt->format) {
    
                    case CS_FMT_NULLTERM:
                        tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() FMT_NULLTERM\n");
                        if (len == destlen ) { 
                           tdsdump_log(TDS_DBG_FUNC, "%L not enough room for data + a null terminator - error\n");
                           ret = CS_FAIL;    /* not enough room for data + a null terminator - error */
                        }
                        else {
                           memcpy(dest, cres.c, len);
			   dest[len] = 0;
                           if (resultlen != (CS_INT *)NULL ) *resultlen = len+1;
                           ret = CS_SUCCEED;
                        }
                        break;
                           
                    case CS_FMT_PADBLANK:
                        tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() FMT_PADBLANK\n");
			/* strcpy here can lead to a small buffer overflow */
                        memcpy(dest, cres.c, len);
                        for ( i = len; i < destlen; i++)
                            dest[i] = ' ';
                        if (resultlen != (CS_INT *)NULL ) *resultlen = destlen;
                        ret = CS_SUCCEED;
                        break;

                    case CS_FMT_PADNULL:
                        tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() FMT_PADNULL\n");
			/* strcpy here can lead to a small buffer overflow */
                        memcpy(dest, cres.c, len);
                        for ( i = len; i < destlen; i++)
                            dest[i] = '\0';
                        if (resultlen != (CS_INT *)NULL ) *resultlen = destlen;
                        ret = CS_SUCCEED;
                        break;
                    case CS_FMT_UNUSED:
                        tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() FMT_UNUSED\n");
                        memcpy(dest, cres.c, len);
                        if (resultlen != (CS_INT *)NULL ) *resultlen = len;
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
    tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() returning  %d\n", ret);
    return (ret);
}
CS_RETCODE cs_dt_crack(CS_CONTEXT *ctx, CS_INT datetype, CS_VOID *dateval, CS_DATEREC *daterec)
{
CS_DATETIME *dt;
CS_DATETIME4 *dt4;
time_t tmp_secs_from_epoch;
struct tm *t;
TDSDATEREC dr;

	if (datetype == CS_DATETIME_TYPE) {
		dt = (TDS_DATETIME *)dateval;
		tds_datecrack(SYBDATETIME, dt, &dr);
	} else if (datetype == CS_DATETIME4_TYPE) {
		dt4 = (TDS_DATETIME4 *)dateval;
		tds_datecrack(SYBDATETIME4, dt4, &dr);
	} else {
		return CS_FAIL;
	}
   	t = (struct tm *) gmtime(&tmp_secs_from_epoch);
	daterec->dateyear   = dr.year;
	daterec->datemonth  = dr.month;
	daterec->datedmonth = dr.day;
	daterec->datedyear  = dr.dayofyear;
	daterec->datedweek  = dr.weekday;
	daterec->datehour   = dr.hour;
	daterec->dateminute = dr.minute;
	daterec->datesecond = dr.second;
	daterec->datetzone   = 0; 

	return CS_SUCCEED;
}
CS_RETCODE cs_loc_alloc(CS_CONTEXT *ctx, CS_LOCALE **locptr)
{ 
	return CS_SUCCEED;
}
CS_RETCODE cs_loc_drop(CS_CONTEXT *ctx, CS_LOCALE *locale)
{ 
	return CS_SUCCEED;
}
CS_RETCODE cs_locale(CS_CONTEXT *ctx, CS_INT action, CS_LOCALE *locale, CS_INT type, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen)
{ 
	return CS_SUCCEED;
}
CS_RETCODE cs_dt_info(CS_CONTEXT *ctx, CS_INT action, CS_LOCALE *locale, CS_INT type, CS_INT item, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen)
{
	if (action==CS_SET) {
		switch(type) {
			case CS_DT_CONVFMT:
				break;
		}
	}
	return CS_SUCCEED;
}

CS_RETCODE
cs_strbuild(CS_CONTEXT *ctx, CS_CHAR *buffer, CS_INT buflen, CS_INT *resultlen, CS_CHAR *text, CS_INT textlen, CS_CHAR *formats, CS_INT formatlen, ...)
{
va_list ap;
CS_RETCODE rc;

	va_start(ap, formatlen);
	rc = tds_vstrbuild(buffer, buflen, resultlen, text, textlen,
				formats, formatlen, ap);
	va_end(ap);
	return rc;
}
