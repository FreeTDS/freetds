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

#include <config.h>
#include <cspublic.h>
#include <tdsconvert.h>
#include <time.h>

static char  software_version[]   = "$Id: cs.c,v 1.10 2002-08-02 03:13:00 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


CS_RETCODE cs_ctx_alloc(CS_INT version, CS_CONTEXT **ctx)
{
TDSCONTEXT *tds_ctx;

	*ctx = (CS_CONTEXT *) malloc(sizeof(CS_CONTEXT));
	memset(*ctx,'\0',sizeof(CS_CONTEXT));
	tds_ctx = tds_alloc_context();
	(*ctx)->tds_ctx = tds_ctx;
	if( tds_ctx->locale && !tds_ctx->locale->date_fmt ) {
		/* set default in case there's no locale file */
		tds_ctx->locale->date_fmt = strdup("%b %e %Y %l:%M%p"); 
	}
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
CS_RETCODE cs_config(CS_CONTEXT *ctx, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen)
{
	return CS_SUCCEED;
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
                             if (resultlen != (CS_INT *)NULL ) *resultlen = src_len;
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

    tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() calling tds_convert\n");
    len = tds_convert(ctx->tds_ctx, src_type, srcdata, src_len, desttype, destlen, &cres);

    if (len == TDS_FAIL)
       return CS_FAIL;
    tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() tds_convert returned %d\n", len);

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
        case SYBBIT:
        case SYBBITN:
             memcpy(dest,&(cres.ti),1);
             if (resultlen != (CS_INT *)NULL ) *resultlen = 1;
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
                        if (strlen(cres.c) == destlen ) { 
                           tdsdump_log(TDS_DBG_FUNC, "%L not enough room for data + a null terminator - error\n");
                           ret = CS_FAIL;    /* not enough room for data + a null terminator - error */
                        }
                        else {
                           strcpy(dest, cres.c);
                           if (resultlen != (CS_INT *)NULL ) *resultlen = strlen(cres.c);
                           ret = CS_SUCCEED;
                        }
                        break;
                           
                    case CS_FMT_PADBLANK:
                        tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() FMT_PADBLANK\n");
                        strcpy(dest, cres.c);
                        for ( i = strlen(cres.c); i < destlen; i++)
                            dest[i] = ' ';
                        if (resultlen != (CS_INT *)NULL ) *resultlen = destlen;
                        ret = CS_SUCCEED;
                        break;

                    case CS_FMT_PADNULL:
                        tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() FMT_PADNULL\n");
                        strcpy(dest, cres.c);
                        for ( i = strlen(cres.c); i < destlen; i++)
                            dest[i] = '\0';
                        if (resultlen != (CS_INT *)NULL ) *resultlen = destlen;
                        ret = CS_SUCCEED;
                        break;
                    case CS_FMT_UNUSED:
                        tdsdump_log(TDS_DBG_FUNC, "%L inside cs_convert() FMT_UNUSED\n");
                        memcpy(dest, cres.c, strlen(cres.c));
                        if (resultlen != (CS_INT *)NULL ) *resultlen = strlen(cres.c);
                        ret = CS_SUCCEED;
                        break;
                    default:
                        ret = CS_FAIL;
                        break;
                }
             }
             free(cres.c);
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

	if (datetype == CS_DATETIME_TYPE) {
	   dt = (TDS_DATETIME *)dateval;
		tmp_secs_from_epoch = ((dt->dtdays - 25567) * (24*60*60)) 
			+ (dt->dttime / 300);
	} else if (datetype == CS_DATETIME4_TYPE) {
		dt4 = (TDS_DATETIME4 *)dateval;
		tmp_secs_from_epoch = ((dt4->days - 25567) * (24*60*60)) 
			+ (dt4->minutes * 60);
	} else {
		return CS_FAIL;
	}
   	t = (struct tm *) gmtime(&tmp_secs_from_epoch);
	daterec->dateyear   = t->tm_year + 1900;
	daterec->datemonth  = t->tm_mon;
	daterec->datedmonth = t->tm_mday;
	daterec->datedyear  = t->tm_yday;
	daterec->datedweek  = t->tm_wday;
	daterec->datehour   = t->tm_hour;
	daterec->dateminute = t->tm_min;
	daterec->datesecond = t->tm_sec;
	daterec->datetzone   = 0; /* ??? */

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
