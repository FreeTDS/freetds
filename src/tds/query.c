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
#include "tds.h"
#include "tdsutil.h"
#include "tdsconvert.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char  software_version[]   = "$Id: query.c,v 1.14 2002-09-25 15:57:14 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

/* All manner of client to server submittal functions */

/* 
** tds_submit_query() sends a language string to the database server for
** processing.  TDS 4.2 is a plain text message with a packet type of 0x01,
** TDS 7.0 is a unicode string with packet type 0x01, and TDS 5.0 uses a 
** TDS_LANG_TOKEN to encapsulate the query and a packet type of 0x0f.
*/
int tds_submit_query(TDSSOCKET *tds, char *query)
{
unsigned char *buf;
int	bufsize;
TDS_INT bufsize2;

	if (!query) return TDS_FAIL;

	/* Jeff's hack to handle long query timeouts */
	tds->queryStarttime = time(NULL); 

	if (tds->state==TDS_PENDING) {
		/* FIX ME -- get real message number et al. 
		** if memory serves the servername is 
		** OpenClient for locally generated messages,
		** but this needs to be verified too.
		*/
		tds_client_msg(tds->tds_ctx, tds,10000,7,0,1,
        "Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}

	tds_free_all_results(tds);

	tds->rows_affected = 0;
	tds->state = TDS_QUERYING;
	if (IS_TDS50(tds)) {
		bufsize = strlen(query)+6;
		buf = (unsigned char *) malloc(bufsize);
		if (!buf) return TDS_FAIL;
		memset(buf,'\0',bufsize);
		buf[0]=TDS_LANG_TOKEN; 

		bufsize2 = strlen(query) + 1;
		memcpy(buf+1, (void *)&bufsize2, 4);

		memcpy(&buf[6],query,strlen(query));
		tds->out_flag=0x0F;
	} else if (IS_TDS70(tds) || IS_TDS80(tds)) {
		bufsize = strlen(query)*2;
		buf = (unsigned char *) malloc(bufsize);
		if (!buf) return TDS_FAIL;
		memset(buf,'\0',bufsize);
		tds7_ascii2unicode(tds,query, buf, bufsize);
		tds->out_flag=0x01;
	} else { /* 4.2 */
		bufsize = strlen(query);
		buf = (unsigned char *) malloc(bufsize);
		if (!buf) return TDS_FAIL;
		memset(buf,'\0',bufsize);
		memcpy(&buf[0],query,strlen(query));
		tds->out_flag=0x01;
	}
	tds_put_n(tds, buf, bufsize);
	tds_flush_packet(tds);
	
	free(buf);

	return TDS_SUCCEED;
}
/* 
** tds_submit_prepare() creates a temporary stored procedure in the server.
** Currently works only with TDS 5.0 
*/
int tds_submit_prepare(TDSSOCKET *tds, char *query, char *id)
{
int id_len, query_len;

	if (!query || !id) return TDS_FAIL;

	if (!IS_TDS50(tds) /* && !IS_TDS7_PLUS(tds) */ ) {
		tds_client_msg(tds->tds_ctx, tds,10000,7,0,1,
        "Dynamic placeholders only supported under TDS 5.0");
		return TDS_FAIL;
	}
	if (tds->state==TDS_PENDING) {
		tds_client_msg(tds->tds_ctx, tds,10000,7,0,1,
        "Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}
	tds_free_all_results(tds);

	/* allocate a structure for this thing */
	tds_alloc_dynamic(tds, id);

	tds->rows_affected = 0;
	tds->state = TDS_QUERYING;
	id_len = strlen(id);
	query_len = strlen(query);

	/* FIXME add support for mssql, use RPC and sp_prepare */
	if (0 && IS_TDS7_PLUS(tds)) {
		int len;

		tds->out_flag = 3; /* RPC */
		/* procedure name */
		tds_put_smallint(tds,10);
		tds_put_n(tds,"s\0p\0_\0p\0r\0e\0p\0a\0r\0e",20);
		tds_put_smallint(tds,0); 

		/* return param handle (int) */
		tds_put_byte(tds,0);
		tds_put_byte(tds,1); /* result */
		tds_put_byte(tds,SYBINTN);
		tds_put_byte(tds,4);
		tds_put_byte(tds,0);
	
		/* string with parameters types */
		tds_put_byte(tds,0);
		tds_put_byte(tds,0);
		tds_put_byte(tds,SYBTEXT); /* ms use ntext but we low bandwidth */
		tds_put_int(tds,len);
		tds_put_int(tds,len);
		/* TODO */
		/* tds_put_n(tds,params_string,len); */
	
		/* string with sql statement */
		/* TODO build param strings from parameters */
		len = strlen(query);
		tds_put_byte(tds,0);
		tds_put_byte(tds,0);
		tds_put_byte(tds,SYBTEXT); /* ms use ntext but we low bandwidth */
		tds_put_int(tds,query_len);
		tds_put_int(tds,query_len);
		tds_put_n(tds,query,query_len);

		/* 1 param ?? why ? */
		tds_put_byte(tds,0);
		tds_put_byte(tds,0);
		tds_put_byte(tds,SYBINT4);
		tds_put_int(tds,1);
		
		tds->out_flag = 0xf; /* default */

		tds_flush_packet(tds);
		return TDS_SUCCEED;
	}

	tds_put_byte(tds,0xe7); 
	tds_put_smallint(tds,query_len + id_len*2 + 21); 
	tds_put_byte(tds,0x01); 
	tds_put_byte(tds,0x00); 
	tds_put_byte(tds,id_len); 
	tds_put_n(tds, id, id_len);
	tds_put_smallint(tds,query_len + id_len + 16); 
	tds_put_n(tds, "create proc ", 12);
	tds_put_n(tds, id, id_len);
	tds_put_n(tds, " as ", 4);
	tds_put_n(tds, query, query_len);

	tds->out_flag=0x0F;
	tds_flush_packet(tds);

	return TDS_SUCCEED;
}
/* 
** tds_submit_execute() sends a previously prepared dynamic statement to the 
** server.
** Currently works only with TDS 5.0 
*/
int tds_submit_execute(TDSSOCKET *tds, char *id)
{
TDSDYNAMIC *dyn;
TDSINPUTPARAM *param;
int elem, id_len;
int i;
int one = 1;

     tdsdump_log(TDS_DBG_FUNC, "%L inside tds_submit_execute() %s\n",id);

	id_len = strlen(id);

     /* FIXME if id is not found ?? */
     elem = tds_lookup_dynamic(tds, id);
     dyn = tds->dyns[elem];

	/* FIXME add support for mssql, use RPC and sp_prepare */
	if (0 && IS_TDS7_PLUS(tds)) {
		/* RPC on sp_execute */
		tds->out_flag = 3; /* RPC */
		/* procedure name */
		tds_put_smallint(tds,10);
		tds_put_n(tds,"s\0p\0_\0e\0x\0e\0c\0u\0t\0e",20);
		tds_put_smallint(tds,0); 
		
		/* id of prepared statement */
		tds_put_byte(tds,0);
		tds_put_byte(tds,0);
		tds_put_byte(tds,SYBINT4);
		tds_put_int(tds,dyn->num_id);

		for (i=0;i<dyn->num_params;i++) {
			param = dyn->params[i];
			tds_put_byte(tds,0x00); 
			tds_put_byte(tds,0x00); 
			tds_put_byte(tds,param->column_type); 
			/* TODO out length correctly based on type */
			if (param->column_bindlen) { 
				tds_put_byte(tds,param->column_bindlen);
				tds_put_byte(tds,param->column_bindlen); 
				tds_put_n(tds, param->varaddr,param->column_bindlen); 
			} else {
				tds_put_byte(tds,0xff);
				tds_put_byte(tds,strlen(param->varaddr)); 
				tds_put_n(tds, param->varaddr,strlen(param->varaddr)); 
			}
		}
		
		tds->out_flag = 0xf; /* default */

		tds_flush_packet(tds);
		return TDS_SUCCEED;
	}

/* dynamic id */
	tds_put_byte(tds,0xe7); 
	tds_put_smallint(tds,id_len + 5); 
	tds_put_byte(tds,0x02); 
	tds_put_byte(tds,0x01); 
	tds_put_byte(tds,id_len); 
	tds_put_n(tds, id, id_len);
	tds_put_byte(tds,0x00); 
	tds_put_byte(tds,0x00); 

/* column descriptions */
	tds_put_byte(tds,0xec); 
	/* size */
	tds_put_smallint(tds, 9 * dyn->num_params + 2); 
	/* number of parameters */
	tds_put_byte(tds,dyn->num_params); 
	/* column detail for each parameter */
	for (i=0;i<dyn->num_params;i++) {
		param = dyn->params[i];
		tds_put_byte(tds,0x00); 
		tds_put_byte(tds,0x00); 
		tds_put_byte(tds,0x00); 
		tds_put_byte(tds,0x00); 
		tds_put_byte(tds,0x00); 
		tds_put_byte(tds,0x00); 
		tds_put_byte(tds,0x00); 
		tds_put_byte(tds,tds_get_null_type(param->column_type)); 
		if (param->column_bindlen) { 
			tds_put_byte(tds,param->column_bindlen);
		} else {
			tds_put_byte(tds,0xff);
		}
	}
	tds_put_byte(tds,0x00); 

/* row data */
	tds_put_byte(tds,0xd7); 
	for (i=0;i<dyn->num_params;i++) {
		param = dyn->params[i];
		if (param->column_bindlen) {
			tds_put_byte(tds,param->column_bindlen); 
			param->varaddr = (char *)&one;
			tds_put_n(tds, param->varaddr,param->column_bindlen); 
		} else {
			tds_put_byte(tds,strlen(param->varaddr)); 
			tds_put_n(tds, param->varaddr,strlen(param->varaddr)); 
		}
	}


/* send it */
	tds->out_flag=0x0F;
	tds_flush_packet(tds);

	return TDS_SUCCEED;
}
/*
** tds_send_cancel() sends an empty packet (8 byte header only)
** tds_process_cancel should be called directly after this.
*/
int tds_send_cancel(TDSSOCKET *tds)
{
	/* TODO discard any partial packet here */
	/* tds_init_write_buf(tds); */

        tds->out_flag=0x06;
        tds_flush_packet(tds);

        return 0;
}

