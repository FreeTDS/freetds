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

#include "tds.h"
#include "tdsutil.h"
#include "tdsconvert.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char  software_version[]   = "$Id: query.c,v 1.23 2002-10-01 15:43:16 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

/* All manner of client to server submittal functions */

/**
 * \defgroup query Query
 * \addtogroup query
 *  \@{ 
 */

/**
 * tds_submit_query() sends a language string to the database server for
 * processing.  TDS 4.2 is a plain text message with a packet type of 0x01,
 * TDS 7.0 is a unicode string with packet type 0x01, and TDS 5.0 uses a 
 * TDS_LANG_TOKEN to encapsulate the query and a packet type of 0x0f.
 * @param query language query to submit
 * @return TDS_FAIL or TDS_SUCCEED
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
		tds_client_msg(tds->tds_ctx, tds, 20019,7,0,1,
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

/**
 * Get position of next placeholders
 * @param start pointer to part of query to search
 * @return next placaholders or NULL if not found
 */
const char*
tds_next_placeholders(const char* start)
{
	const char *p = start;
	char quote = 0;

	if (!p) return NULL;

	for(;*p;++p) {
		switch(*p) {
		case '\'':
		case '\"':
		case ']':
			if (!quote) {
				quote = *p;
			} else if (*p == quote) {
				if (p[1] == quote) ++p;
				else quote = 0;
			}
			break;
		case '[':
			if (!quote)
				quote = ']';
			break;
		case '?':
			if (!quote) return p;
		default:
		}
	}
	return NULL;
}

/**
 * Count the number of placeholders in query
 */
static int 
tds_count_placeholders(const char *query)
{
	const char *p = query-1;
	int count = 0;
	for(;;++count) {
		if (!(p=tds_next_placeholders(p+1)))
			return count;
	}
}

/**
 * tds_submit_prepare() creates a temporary stored procedure in the server.
 * Currently works only with TDS 5.0 (work in progress for TDS7+)
 * @param query language query with given placeholders (?)
 * @param id string to identify the dynamic query
 * @return TDS_FAIL or TDS_SUCCEED
 */
int tds_submit_prepare(TDSSOCKET *tds, char *query, char *id)
{
int id_len, query_len;

	if (!query || !id) return TDS_FAIL;

	if (!IS_TDS50(tds) && !IS_TDS7_PLUS(tds)) {
		tdsdump_log(TDS_DBG_ERROR,
			"Dynamic placeholders only supported under TDS 5.0 and TDS 7.0+\n");
		return TDS_FAIL;
	}
	if (tds->state==TDS_PENDING) {
		tds_client_msg(tds->tds_ctx, tds,20019,7,0,1,
			"Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}
	tds_free_all_results(tds);

	/* allocate a structure for this thing */
	if (!tds_alloc_dynamic(tds, id))
		return TDS_FAIL;

	/* FIXME is a bit ugly allocate, give a position and then search again ...*/
	tds->cur_dyn_elem = tds_lookup_dynamic(tds, id);

	tds->rows_affected = 0;
	tds->state = TDS_QUERYING;
	id_len = strlen(id);
	query_len = strlen(query);

	/* FIXME add support for mssql, use RPC and sp_prepare */
	if (IS_TDS7_PLUS(tds)) {
		int len,i,n;
		const char *s,*e;

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
		tds_put_byte(tds,SYBNTEXT); /* must be Ntype */
		/* TODO build true param string from parameters */
		/* for now we use all "@PX varchar(80)," for parameters */
		n = tds_count_placeholders(query);
		len = n * 16 -1;
		/* adjust for the length of X */
		for(i=10;i<=n;i*=10) {
			len += n-i+1;
		}
		tds_put_int(tds,len*2);
		tds_put_int(tds,len*2);
		for (i=1;i<=n;++i) {
			char buf[24];
			sprintf(buf,"%s@P%d varchar(80)",(i==1?"":","),i);
			tds_put_string(tds,buf,-1);
		}
	
		/* string with sql statement */
		/* replace placeholders with dummy parametes */
		tds_put_byte(tds,0);
		tds_put_byte(tds,0);
		tds_put_byte(tds,SYBNTEXT); /* must be Ntype */
		len = (len+1-14*n)+query_len;
		tds_put_int(tds,len*2);
		tds_put_int(tds,len*2);
		s = query;
		for(i=1;i<=n;++i) {
			char buf[24];
			e = tds_next_placeholders(s);
			tds_put_string(tds,s,e?e-s:strlen(s));
			sprintf(buf,"@P%d",i);
			tds_put_string(tds,buf,-1);
			if (!e) break;
			s = e+1;
		}

		/* 1 param ?? why ? */
		tds_put_byte(tds,0);
		tds_put_byte(tds,0);
		tds_put_byte(tds,SYBINT4);
		tds_put_int(tds,1);
		
		tds_flush_packet(tds);

		tds->out_flag = 0xf; /* default */
		return TDS_SUCCEED;
	}

	tds->out_flag=0x0F;

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

	tdsdump_log(TDS_DBG_FUNC, "%L inside tds_submit_execute() %s\n",id);

	id_len = strlen(id);

	elem = tds_lookup_dynamic(tds, id);
	if (elem < 0) return TDS_FAIL;
	dyn = tds->dyns[elem];

	/* FIXME add support for mssql, use RPC and sp_prepare */
	if (IS_TDS7_PLUS(tds)) {
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
			tds_put_byte(tds,0x00); /* no param name */
			tds_put_byte(tds,0x00); /* input */
			tds_put_byte(tds,tds_get_null_type(param->column_type));
			/* TODO out length correctly based on type use a "tds_put_data" */
			/* this work on small string... */
			if (param->column_bindlen) { 
				tds_put_byte(tds,param->column_bindlen);
				tds_put_byte(tds,param->column_bindlen); 
				tds_put_n(tds, param->varaddr,param->column_bindlen); 
			} else {
				tds_put_byte(tds,0xff);
				/* FIXME database strings are not null terminated !!! */
				tds_put_byte(tds,strlen(param->varaddr)); 
				tds_put_n(tds, param->varaddr,strlen(param->varaddr)); 
			}
		}
		
		tds_flush_packet(tds);

		tds->out_flag = 0xf; /* default */
		return TDS_SUCCEED;
	}

	tds->out_flag=0x0F;
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
	tds_put_smallint(tds,dyn->num_params); 
	/* column detail for each parameter */
	for (i=0;i<dyn->num_params;i++) {
		param = dyn->params[i];
		tds_put_byte(tds,0x00); /* param name len*/
		tds_put_byte(tds,0x00); /* status (input) */
		tds_put_int(tds,0); /* usertype */
		tds_put_byte(tds,tds_get_null_type(param->column_type)); 
		/* FIXME handle larger types */
		if (param->column_bindlen) { 
			tds_put_byte(tds,param->column_bindlen);
		} else {
			tds_put_byte(tds,0xff);
		}
		tds_put_byte(tds,0x00); /* locale info length */
	}

/* row data */
	tds_put_byte(tds,0xd7); 
	for (i=0;i<dyn->num_params;i++) {
		param = dyn->params[i];
		if (param->column_bindlen) {
			tds_put_byte(tds,param->column_bindlen);
			tds_put_n(tds, param->varaddr,param->column_bindlen); 
		} else {
			tds_put_byte(tds,strlen(param->varaddr)); 
			tds_put_n(tds, param->varaddr,strlen(param->varaddr)); 
		}
	}

/* send it */
	tds_flush_packet(tds);

	return TDS_SUCCEED;
}

static volatile int inc_num = 1;
/**
 * Get an id for dynamic query based on TDS information
 * @return TDS_FAIL or TDS_SUCCEED
 */
int
tds_get_dynid(TDSSOCKET *tds,char **id)
{
	inc_num = (inc_num+1) & 0xffff;
	if (asprintf(id,"dyn%x_%d",(int)tds,inc_num)<0) return TDS_FAIL;
	return TDS_SUCCEED;
}

/**
 * Free given prepared query
 */
int tds_submit_unprepare(TDSSOCKET *tds, char *id)
{
TDSDYNAMIC *dyn;
int elem, id_len;

	tdsdump_log(TDS_DBG_FUNC, "%L inside tds_submit_unprepare() %s\n",id);

	id_len = strlen(id);

	elem = tds_lookup_dynamic(tds, id);
	if (elem < 0) return TDS_FAIL;
	dyn = tds->dyns[elem];
	
	/* TODO continue ...*/
	return TDS_FAIL;
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

/** \@} */

