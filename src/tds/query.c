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

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#include "tdsconvert.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include <assert.h>

static char  software_version[]   = "$Id: query.c,v 1.53 2002-11-29 10:29:36 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

/* All manner of client to server submittal functions */

/**
 * \defgroup query Query
 * Function to handle query.
 */

/**
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
int tds_submit_query(TDSSOCKET *tds, const char *query)
{
int	query_len;

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

	query_len = strlen(query);
	if (IS_TDS50(tds)) {
		tds->out_flag = 0x0F;
		tds_put_byte(tds, TDS_LANG_TOKEN);
		tds_put_int(tds, query_len+1);
		tds_put_byte(tds, 0);
		tds_put_n(tds, query, query_len);
	} else {
		tds->out_flag = 0x01;
		tds_put_string(tds, query, query_len);
	}
	return tds_flush_packet(tds);
}

int
tds_submit_queryf(TDSSOCKET *tds, const char *queryf, ...)
{
va_list ap;
char *query = NULL;
int rc = TDS_FAIL;

	va_start(ap, queryf);
	if (vasprintf(&query, queryf, ap) >= 0) {
		rc = tds_submit_query(tds, query);
		free(query);
	}
	va_end(ap);
	return rc;
}


/**
 * Skip quoting string (like 'sfsf', "dflkdj" or [dfkjd])
 * @param s pointer to first quoting character (should be '," or [)
 * @return character after quoting
 */
static const char* 
tds_skip_quoted(const char* s)
{
	const char *p = s;
	char quote = (*s == '[') ? ']' : *s;
	for(;*++p;) {
		if (*p == quote) {
			if (*++p != quote)
				return p;
		}
	}
	return p;
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

	if (!p) return NULL;

	for(;;) {
		switch(*p) {
		case '\0':
			return NULL;
		case '\'':
		case '\"':
		case '[':
			p = tds_skip_quoted(p);
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
	const char *p = query-1;
	int count = 0;
	for(;;++count) {
		if (!(p=tds_next_placeholders(p+1)))
			return count;
	}
}

/**
 * tds_submit_prepare() creates a temporary stored procedure in the server.
 * Currently works with TDS 5.0 and TDS7+
 * @param query language query with given placeholders (?)
 * @param id string to identify the dynamic query. Pass NULL for automatic generation.
 * @param dyn_out will receive allocated TDSDYNAMIC*. Any older allocated dynamic won't be freed, Can be NULL.
 * @return TDS_FAIL or TDS_SUCCEED
 */
/* TODO parse all results ?? */
int 
tds_submit_prepare(TDSSOCKET *tds, const char *query, const char *id, TDSDYNAMIC **dyn_out)
{
int id_len, query_len;
TDSDYNAMIC *dyn;

	if (!query) return TDS_FAIL;

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
	if (!id) {
		char *tmp_id = NULL;
		if (tds_get_dynid(tds,&tmp_id) == TDS_FAIL)
			return TDS_FAIL;
		dyn = tds_alloc_dynamic(tds, tmp_id);
		TDS_ZERO_FREE(tmp_id);
		if (!dyn)
			return TDS_FAIL;
	} else {
		dyn = tds_alloc_dynamic(tds, id);
	}
	if (!dyn)
		return TDS_FAIL;

	tds->cur_dyn = dyn;

	if (dyn_out)
		*dyn_out = dyn;

	tds->rows_affected = 0;
	tds->state = TDS_QUERYING;
	query_len = strlen(query);

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
		/* for now we use all "@PX varchar(80)," for parameters (same behavior of mssql2k) */
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
		/* TODO do a test with "...?" and "...?)" */
		for(i=1;;++i) {
			char buf[24];
			e = tds_next_placeholders(s);
			tds_put_string(tds,s,e?e-s:strlen(s));
			if (!e) break;
			sprintf(buf,"@P%d",i);
			tds_put_string(tds,buf,-1);
			s = e+1;
		}

		/* 1 param ?? why ? flags ?? */
		tds_put_byte(tds,0);
		tds_put_byte(tds,0);
		tds_put_byte(tds,SYBINT4);
		tds_put_int(tds,1);
		
		return tds_flush_packet(tds);
	}

	tds->out_flag=0x0F;

	id_len = strlen(dyn->id);
	tds_put_byte(tds, TDS5_DYN_TOKEN); 
	tds_put_smallint(tds,query_len + id_len*2 + 21); 
	tds_put_byte(tds,0x01); 
	tds_put_byte(tds,0x00); 
	tds_put_byte(tds,id_len); 
	tds_put_n(tds, dyn->id, id_len);
	tds_put_smallint(tds,query_len + id_len + 16); 
	tds_put_n(tds, "create proc ", 12);
	tds_put_n(tds, dyn->id, id_len);
	tds_put_n(tds, " as ", 4);
	tds_put_n(tds, query, query_len);

	return tds_flush_packet(tds);
}


#define TDS_PUT_DATA_USE_NAME 1
/**
 * Put data information to wire
 * @param curcol column where to store information
 * @param flags  bit flags on how to send data (use TDS_PUT_DATA_USE_NAME for use name information)
 * @return TDS_SUCCEED or TDS_FAIL
 */
/* TODO add a flag for select if named used or not ?? */
static int 
tds_put_data_info(TDSSOCKET *tds, TDSCOLINFO *curcol, int flags)
{
	if (flags & TDS_PUT_DATA_USE_NAME) {
		/* TODO use column_namelen ?? */
		tds_put_byte(tds, strlen(curcol->column_name)); /* param name len*/
		tds_put_string(tds, curcol->column_name, strlen(curcol->column_name));
	} else {
		tds_put_byte(tds,0x00); /* param name len*/
	}
	/* TODO store and use flags (output/use defaul null)*/
	tds_put_byte(tds,0x00); /* status (input) */
	if (!IS_TDS7_PLUS(tds))
		tds_put_int(tds,curcol->column_usertype); /* usertype */
	tds_put_byte(tds, curcol->column_type); 
	if (is_numeric_type(curcol->column_type)) {
		tds_put_byte(tds, curcol->column_prec);
		tds_put_byte(tds, curcol->column_scale);
	}
	switch(curcol->column_varint_size) {
	case 0:
		break;
	case 1:
		tds_put_byte(tds, curcol->column_cur_size);
		break;
	case 2:
		tds_put_smallint(tds, curcol->column_cur_size);
		break;
	case 4:
		tds_put_int(tds, curcol->column_cur_size);
		break;
	}
	/* TODO needed in TDS4.2 ?? now is called only is TDS >= 5 */
	if (!IS_TDS7_PLUS(tds))
		tds_put_byte(tds,0x00); /* locale info length */
	return TDS_SUCCEED;
}

/**
 * Calc information length in bytes (useful for calculating full packet length)
 * @param curcol column where to store information
 * @param flags  bit flags on how to send data (use TDS_PUT_DATA_USE_NAME for use name information)
 * @return TDS_SUCCEED or TDS_FAIL
 */
static int 
tds_put_data_info_length(TDSSOCKET *tds, TDSCOLINFO *curcol, int flags)
{
int len = 8;

#ifdef ENABLE_EXTRA_CHECKS
	if (IS_TDS7_PLUS(tds))
		tdsdump_log(TDS_DBG_ERROR, "%L tds_put_data_info_length called with TDS7+\n");
#endif

	if (flags & TDS_PUT_DATA_USE_NAME)
		/* TODO use column_namelen ? */
		len += strlen(curcol->column_name);
	if (is_numeric_type(curcol->column_type))
		len += 2;
	return len + curcol->column_varint_size;
}

/**
 * Write data to wire
 * @param curcol column where store column information
 * @param pointer to row data to store information
 * @param i column position in current_row
 * @return TDS_FAIL on error or TDS_SUCCEED
 */
static int 
tds_put_data(TDSSOCKET *tds,TDSCOLINFO *curcol,unsigned char *current_row, int i)
{
unsigned char *dest;
TDS_NUMERIC *num;
TDSBLOBINFO *blob_info;
int colsize;
int is_null;

	is_null = tds_get_null(current_row,i);
	colsize = curcol->column_cur_size;

	/* put size of data*/
	switch (curcol->column_varint_size) {
	case 4: /* Its a BLOB... */
		blob_info = (TDSBLOBINFO *) &(current_row[curcol->column_offset]);
		if (!is_null) {
			tds_put_byte(tds, 16);
			tds_put_n(tds, blob_info->textptr,16);
			tds_put_n(tds, blob_info->timestamp,8);
			tds_put_int(tds, colsize);
		} else {
			tds_put_byte(tds, 0);
		}
		break;
	case 2:
		if (!is_null) tds_put_smallint(tds, colsize);
		else tds_put_smallint(tds, -1);
		break;
	case 1: 
		if (!is_null) tds_put_byte(tds, colsize);
		else tds_put_byte(tds, 0);
		break;
	case 0: 
		colsize = tds_get_size_by_type(curcol->column_type);
		break;
	}
	
	if (is_null)
		return TDS_SUCCEED;

	/* put real data */
	if (is_numeric_type(curcol->column_type)) {
		/* TODO use TDS7 and swap for big endian */
		num = (TDS_NUMERIC *) &(current_row[curcol->column_offset]);
		/* TODO colsize is correct here ?? */
		tds_put_n(tds,num->array,colsize);
	} else if (is_blob_type(curcol->column_type)) {
		blob_info = (TDSBLOBINFO *) &(current_row[curcol->column_offset]);
		tds_put_n(tds, blob_info->textvalue, colsize);
	} else {
		/* FIXME problem with big endian, swap data */
		dest = &(current_row[curcol->column_offset]);
		tds_put_n(tds,dest,colsize);
	}
	return TDS_SUCCEED;
}

/**
 * tds_submit_execute() sends a previously prepared dynamic statement to the 
 * server.
 * Currently works with TDS 5.0 or TDS7+
 * @param dyn dynamic proc to execute. Must build from same tds.
 */
int tds_submit_execute(TDSSOCKET *tds, TDSDYNAMIC *dyn)
{
TDSCOLINFO *param;
TDSPARAMINFO *info;
int id_len;
int i, len;

	tdsdump_log(TDS_DBG_FUNC, "%L inside tds_submit_execute()\n");

	if (tds->state==TDS_PENDING) {
		tds_client_msg(tds->tds_ctx, tds,20019,7,0,1,
			"Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}

	/* TODO check this code, copied from tds_submit_prepare */
	tds_free_all_results(tds);
	tds->rows_affected = 0;
	tds->state = TDS_QUERYING;

	tds->cur_dyn = dyn;

	if (IS_TDS7_PLUS(tds)) {
		/* RPC on sp_execute */
		tds->out_flag = 3; /* RPC */
		/* procedure name */
		tds_put_smallint(tds,10);
		tds_put_n(tds,"s\0p\0_\0e\0x\0e\0c\0u\0t\0e",20);
		tds_put_smallint(tds,0); /* flags */
		
		/* id of prepared statement */
		tds_put_byte(tds,0);
		tds_put_byte(tds,0);
		tds_put_byte(tds,SYBINT4);
		tds_put_int(tds,dyn->num_id);

		info = dyn->params;
		for (i=0;i<info->num_cols;i++) {
			param = info->columns[i];
			tds_put_data_info(tds, param, 0);
			tds_put_data(tds, param, info->current_row, i);
		}

		return tds_flush_packet(tds);
	}

	tds->out_flag=0x0F;
	/* dynamic id */
	id_len = strlen(dyn->id);

	tds_put_byte(tds,TDS5_DYN_TOKEN); 
	tds_put_smallint(tds,id_len + 5); 
	tds_put_byte(tds,0x02); 
	tds_put_byte(tds,0x01); 
	tds_put_byte(tds,id_len); 
	tds_put_n(tds, dyn->id, id_len);
	tds_put_byte(tds,0x00); 
	tds_put_byte(tds,0x00); 

	/* TODO use this code in RPC too ?? */

	/* column descriptions */
	tds_put_byte(tds,TDS5_PARAMFMT_TOKEN); 
	/* size */
	len = 2;
	info = dyn->params;
	for (i=0;i<info->num_cols;i++)
		len += tds_put_data_info_length(tds, info->columns[i], 0);
	tds_put_smallint(tds, len);
	/* number of parameters */
	tds_put_smallint(tds,info->num_cols); 
	/* column detail for each parameter */
	for (i=0;i<info->num_cols;i++) {
		/* FIXME add error handling */
		tds_put_data_info(tds, info->columns[i], 0);
	}

	/* row data */
	tds_put_byte(tds,TDS5_PARAMS_TOKEN); 
	for (i=0;i<info->num_cols;i++) {
		tds_put_data(tds, info->columns[i], info->current_row, i);
	}

	/* send it */
	return tds_flush_packet(tds);
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
	if (asprintf(id,"dyn%lx_%d",(long)tds,inc_num)<0) return TDS_FAIL;
	return TDS_SUCCEED;
}

/**
 * Free given prepared query
 */
int tds_submit_unprepare(TDSSOCKET *tds, TDSDYNAMIC *dyn)
{
	if (!dyn) return TDS_FAIL;

	tdsdump_log(TDS_DBG_FUNC, "%L inside tds_submit_unprepare() %s\n", dyn->id);

	/* TODO continue ... reset dynamic, free structure, unprepare to server */
	return TDS_FAIL;
}

/**
 * tds_submit_rpc() call a RPC from server. Output parameters will be stored in tds->param_info
 * @param rpc_name name of RPC
 * @param params   parameters informations
 */
int 
tds_submit_rpc(TDSSOCKET *tds, const char *rpc_name, TDSPARAMINFO *params)
{
	TDSCOLINFO *param;
	int rpc_name_len, i;

	assert(tds);
	assert(rpc_name);
	assert(params);

	if (tds->state==TDS_PENDING) {
		tds_client_msg(tds->tds_ctx, tds,20019,7,0,1,
			"Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}

	tds_free_all_results(tds);
	tds->rows_affected = 0;
	tds->state = TDS_QUERYING;

	/* distinguish from dynamic query  */
	tds->cur_dyn = NULL;

	rpc_name_len = strlen(rpc_name);
	if (IS_TDS7_PLUS(tds)) {
		tds->out_flag = 3; /* RPC */
		/* procedure name */
		tds_put_smallint(tds, rpc_name_len);
		tds_put_string(tds, rpc_name, rpc_name_len);
		tds_put_smallint(tds,0); 
		
		for (i=0;i<params->num_cols;i++) {
			param = params->columns[i];
			tds_put_data_info(tds, param, TDS_PUT_DATA_USE_NAME);
			tds_put_data(tds, param, params->current_row, i);
		}

		return tds_flush_packet(tds);
	}
	
	/* TODO continue, support for TDS5 */
	return TDS_FAIL;
}

/**
 * tds_send_cancel() sends an empty packet (8 byte header only)
 * tds_process_cancel should be called directly after this.
 */
int 
tds_send_cancel(TDSSOCKET *tds)
{
	/* TODO discard any partial packet here */
	/* tds_init_write_buf(tds); */

	tds->out_flag=0x06;
	return tds_flush_packet(tds);
}

/** \@} */

