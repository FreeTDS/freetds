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
#include <time.h>

static char  software_version[]   = "$Id: cs.c,v 1.2 2001-10-24 23:19:44 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


CS_RETCODE cs_ctx_alloc(CS_INT version, CS_CONTEXT **ctx)
{
	*ctx = (CS_CONTEXT *) malloc(sizeof(CS_CONTEXT));
	return CS_SUCCEED;
}
CS_RETCODE cs_ctx_drop(CS_CONTEXT *ctx)
{
	if (ctx) free(ctx);
	return CS_SUCCEED;
}
CS_RETCODE cs_config(CS_CONTEXT *ctx, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen)
{
	return CS_SUCCEED;
}
CS_RETCODE cs_convert(CS_CONTEXT *ctx, CS_DATAFMT *srcfmt, CS_VOID *srcdata, CS_DATAFMT *destfmt, CS_VOID *destdata, CS_INT *resultlen)
{
int src_type, dest_type;

	src_type = _ct_get_server_type(srcfmt->datatype);
	dest_type = _ct_get_server_type(destfmt->datatype);

	tds_convert(src_type, srcdata, srcfmt ? srcfmt->maxlength : 0, 
		dest_type, destdata, destfmt ? destfmt->maxlength : 0);
	return CS_SUCCEED;
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
