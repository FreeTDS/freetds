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
#endif /* HAVE_CONFIG_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "tds.h"
#include "tdsiconv.h"
#include "tdsstring.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char  software_version[]   = "$Id: mem.c,v 1.44 2002-11-01 22:51:35 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


TDSENVINFO *tds_alloc_env(TDSSOCKET *tds);
void tds_free_env(TDSSOCKET *tds);

#undef TEST_MALLOC
#define TEST_MALLOC(dest,type) \
	{if (!(dest = (type*)malloc(sizeof(type)))) goto Cleanup;}

#undef TEST_MALLOCN
#define TEST_MALLOCN(dest,type,n) \
	{if (!(dest = (type*)malloc(sizeof(type)*n))) goto Cleanup;}

#undef TEST_STRDUP
#define TEST_STRDUP(dest,str) \
	{if (!(dest = strdup(str))) goto Cleanup;}

/* TODO do a best check for alignment than this, duplicate from token.c */
union { void *p; int i; } align_struct;
#define ALIGN_SIZE sizeof(align_struct)

/**
 * \defgroup mem Memory allocation
 * Allocate or free resources. Allocation can fail only on out of memory. 
 * In such case they return NULL and leave the state as before call.
 */

/** \addtogroup mem
 *  \@{ 
 */

/** \fn TDSDYNAMIC *tds_alloc_dynamic(TDSSOCKET *tds, char *id)
 *  \brief Allocate a dynamic statement.
 *  \param tds the connection within which to allocate the statement.
 *  \param id a character label identifying the statement.
 *  \return a pointer to the allocated structure (NULL on failure).
 *
 *  tds_alloc_dynamic is used to implement placeholder code under TDS 5.0
 */
TDSDYNAMIC *tds_alloc_dynamic(TDSSOCKET *tds, char *id)
{
int i;
TDSDYNAMIC *dyn;
TDSDYNAMIC **dyns;

	/* check to see if id already exists (shouldn't) */
	for (i=0;i<tds->num_dyns;i++) {
		if (!strcmp(tds->dyns[i]->id, id)) {
			/* id already exists! just return it */
			return(tds->dyns[i]);
		}
	}

	dyn = (TDSDYNAMIC *) malloc(sizeof(TDSDYNAMIC));
	if (!dyn) return NULL;
	memset(dyn, 0, sizeof(TDSDYNAMIC));

	if (!tds->num_dyns) {
		/* if this is the first dynamic stmt */
		dyns = (TDSDYNAMIC **) malloc(sizeof(TDSDYNAMIC *));
	} else {
		/* ok, we have a list and need to add another */
		dyns = (TDSDYNAMIC **) realloc(tds->dyns, 
				sizeof(TDSDYNAMIC *) * (tds->num_dyns+1));
	}

	if (!dyns) {
		free(dyn);
		return NULL;
	}
	tds->dyns = dyns;
	tds->dyns[tds->num_dyns] = dyn;
	strncpy(dyn->id, id, TDS_MAX_DYNID_LEN);
	dyn->id[TDS_MAX_DYNID_LEN-1]='\0';

	return tds->dyns[tds->num_dyns++];
}

/** \fn TDSINPUTPARAM *tds_add_input_param(TDSDYNAMIC *dyn)
 *  \brief Allocate a dynamic statement.
 *  \param dyn the dynamic statement to bind this parameter to.
 *  \return a pointer to the allocated structure (NULL on failure).
 *
 *  tds_add_input_param adds a parameter to a dynamic statement.
 */
/* TDSINPUTPARAM *tds_add_input_param(TDSDYNAMIC *dyn)
{
TDSINPUTPARAM *param;
TDSINPUTPARAM **params;

	param = (TDSINPUTPARAM *) malloc(sizeof(TDSINPUTPARAM));
	if (!param) return NULL;
	memset(param,'\0',sizeof(TDSINPUTPARAM));

	if (!dyn->num_params) {
		params = (TDSINPUTPARAM **) 
			malloc(sizeof(TDSINPUTPARAM *));
	} else {
		params = (TDSINPUTPARAM **) 
			realloc(dyn->params, 
			sizeof(TDSINPUTPARAM *) * (dyn->num_params+1));
	}
	if (!params) {
		free(param);
		return NULL;
	}
	dyn->params = params;
	dyn->params[dyn->num_params] = param;
	dyn->num_params++;
	return param;
}
*/

/** \fn void tds_free_input_params(TDSDYNAMIC *dyn)
 *  \brief Frees all allocated input parameters of a dynamic statement.
 *  \param dyn the dynamic statement whose input parameter are to be freed
 *
 *  tds_free_input_params frees all parameters for the give dynamic statement
 */
void tds_free_input_params(TDSDYNAMIC *dyn)
{
TDSPARAMINFO *info;

	info = dyn->new_params;
	if (info) {
		if (info->columns) free(info->columns);
		if (info->current_row) free(info->current_row);
		free(info);
		dyn->new_params = NULL;
	}
}

/** \fn void tds_free_dynamic(TDSSOCKET *tds)
 *  \brief Frees all dynamic statements for a given connection.
 *  \param tds the connection containing the dynamic statements to be freed.
 *
 *  tds_free_dynamic frees all dynamic statements for the given TDS socket and
 *  then zeros tds->dyns.
 */
void tds_free_dynamic(TDSSOCKET *tds)
{
int i;
TDSDYNAMIC *dyn;

	for (i=0;i<tds->num_dyns;i++) {
		dyn = tds->dyns[i];
		tds_free_input_params(dyn);
		free(dyn);
	}
	if (tds->dyns) TDS_ZERO_FREE(tds->dyns);
	tds->num_dyns = 0;
	
	return;
}
/** \fn TDSPARAMINFO *tds_alloc_param_result(TDSPARAMINFO *old_param)
 *  \brief Adds a output parameter to TDSPARAMINFO.
 *  \param old_param a pointer to the TDSPARAMINFO structure containing the 
 *  current set of output parameter, or NULL if none exists.
 *  \return a pointer to the new TDSPARAMINFO structure.
 *
 *  tds_alloc_param_result() works a bit differently than the other alloc result
 *  functions.  Output parameters come in individually with no total number 
 *  given in advance, so we simply call this func every time with get a
 *  TDS_PARAM_TOKEN and let it realloc the columns struct one bigger. 
 *  tds_free_all_results() usually cleans up after us.
 */
TDSPARAMINFO *tds_alloc_param_result(TDSPARAMINFO *old_param)
{
TDSPARAMINFO *param_info;
TDSCOLINFO *colinfo;
TDSCOLINFO **cols;

	colinfo = (TDSCOLINFO *) malloc(sizeof(TDSCOLINFO));
	if (!colinfo) return NULL;
	memset(colinfo,0,sizeof(TDSCOLINFO));

	if (!old_param || !old_param->num_cols) {
		cols = (TDSCOLINFO **) malloc(sizeof(TDSCOLINFO *));
	} else {
		cols = (TDSCOLINFO **) realloc(old_param->columns, 
				sizeof(TDSCOLINFO *) * (old_param->num_cols+1));
	}
	if (!cols) goto Cleanup;

	if (!old_param) {
		param_info = (TDSPARAMINFO *) malloc(sizeof(TDSPARAMINFO));
		if (!param_info) {
			free(cols);
			goto Cleanup;
		}
		memset(param_info,'\0',sizeof(TDSPARAMINFO));
	} else {
		param_info = old_param;
	}
	param_info->columns = cols;
	param_info->columns[param_info->num_cols++] = colinfo;
	return param_info;
Cleanup:
	free(colinfo);
	return NULL;
}

/**
 * Add another field to row. Is assumed that last TDSCOLINFO contain information about this.
 * Update also info structure.
 * @param info     parameters info where is contained row
 * @param curparam parameter to retrieve size information
 * @return NULL on failure or new row
 */
unsigned char *
tds_alloc_param_row(TDSPARAMINFO *info,TDSCOLINFO *curparam)
{
int null_size, remainder, i;
TDS_INT row_size;
unsigned char *row;

	null_size = (unsigned)(info->num_cols+(8*ALIGN_SIZE-1)) / 8u;
	null_size = null_size - null_size % ALIGN_SIZE;
	null_size -= info->null_info_size;
	if (null_size < 0) null_size = 0;

	curparam->column_offset = info->row_size;
	/* the +1 are needed for terminater... still required (freddy77) */
	row_size += info->row_size + curparam->column_size + 1 + null_size;
	remainder = row_size % ALIGN_SIZE; 
	if (remainder) row_size += (ALIGN_SIZE - remainder);

	/* make sure the row buffer is big enough */
	if (info->current_row) {
		row = realloc(info->current_row, row_size);
	} else {
		row = (void *)malloc(row_size);
	}
	if (!row) return NULL;
	info->current_row = row;
	info->row_size = row_size;

	/* expand null buffer */
	if (null_size) {
		memmove(row+info->null_info_size+null_size,
			row+info->null_info_size,
			row_size-null_size-info->null_info_size);
		memset(row+info->null_info_size,0,null_size);
		info->null_info_size += null_size;
		for(i=0;i<info->num_cols;++i) {
			info->columns[i]->column_offset += null_size;
		}
	}

	return row;
}

/**
 * Allocate memory for storing compute info
 * return NULL on out of memory
 */

static TDSCOMPUTEINFO *
tds_alloc_compute_result(int num_cols, int by_cols)
{
int col;
TDSCOMPUTEINFO *info;

	TEST_MALLOC(info, TDSCOMPUTEINFO);
	memset(info,'\0',sizeof(TDSCOMPUTEINFO));

	TEST_MALLOCN(info->columns, TDSCOLINFO*, num_cols);
	memset(info,'\0',sizeof(TDSCOLINFO*) * num_cols);

	tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. point 1\n");
	info->num_cols = num_cols;
	for (col = 0; col < num_cols; col++)  {
		TEST_MALLOC(info->columns[col], TDSCOLINFO);
		memset(info->columns[col],'\0',sizeof(TDSCOLINFO));
	}

	tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. point 2\n");

	if (by_cols) {
		TEST_MALLOCN(info->bycolumns, TDS_TINYINT, by_cols);
		memset(info->bycolumns,'\0', by_cols);
		tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. point 3\n");
		info->by_cols = by_cols;
	}
	return info;
Cleanup:
	tds_free_compute_result(info);
	return NULL;
}

TDSCOMPUTEINFO **tds_alloc_compute_results(TDS_INT *num_comp_results, TDSCOMPUTEINFO** ci, int num_cols, int by_cols)
{
int n;
TDSCOMPUTEINFO **comp_info;
TDSCOMPUTEINFO *cur_comp_info;

	tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. num_cols = %d bycols = %d\n", num_cols, by_cols);
	tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. num_comp_results = %d\n", *num_comp_results);

	cur_comp_info = tds_alloc_compute_result(num_cols, by_cols);
	if (!cur_comp_info) return NULL;

	n = *num_comp_results;
	if (n == 0)
		comp_info  = (TDSCOMPUTEINFO **) malloc(sizeof(TDSCOMPUTEINFO *));
	else
		comp_info = (TDSCOMPUTEINFO **) realloc(ci, sizeof(TDSCOMPUTEINFO *) * (n+1));

	if (!comp_info) {
		tds_free_compute_result(cur_comp_info);
		return NULL;
	}

	comp_info[n] = cur_comp_info;
	*num_comp_results = n + 1;

	tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. num_comp_results = %d\n", *num_comp_results);

	return comp_info;
}

TDSRESULTINFO *tds_alloc_results(int num_cols)
{
/*TDSCOLINFO *curcol;
 */
TDSRESULTINFO *res_info;
int col;
int null_sz;

	TEST_MALLOC(res_info, TDSRESULTINFO);
	memset(res_info,'\0',sizeof(TDSRESULTINFO));
	TEST_MALLOCN(res_info->columns, TDSCOLINFO *,num_cols);
	for (col=0;col<num_cols;col++)  {
		TEST_MALLOC(res_info->columns[col], TDSCOLINFO);
		memset(res_info->columns[col],'\0',sizeof(TDSCOLINFO));
	}
	res_info->num_cols = num_cols;
	null_sz = (num_cols/8) + 1;
	if (null_sz % ALIGN_SIZE) null_sz = ((null_sz/ALIGN_SIZE)+1)*ALIGN_SIZE;
	res_info->null_info_size = null_sz;
	/* set the initial row size to the size of the null info */
	res_info->row_size = res_info->null_info_size;
	return res_info;
Cleanup:
	tds_free_results(res_info);
	return NULL;
}

/**
 * Allocate space for row store
 * return NULL on out of memory
 */
void *tds_alloc_row(TDSRESULTINFO *res_info)
{
void *ptr;

	ptr = (void *) malloc(res_info->row_size);
	if (!ptr) return NULL;
	memset(ptr,'\0',res_info->row_size); 
	return ptr;
}

void *tds_alloc_compute_row(TDSCOMPUTEINFO *res_info)
{
void *ptr;

	ptr = (void *) malloc(res_info->row_size);
	if (!ptr) return NULL;
	memset(ptr,'\0',res_info->row_size); 
	return ptr;
}

void tds_free_param_results(TDSPARAMINFO *param_info)
{
int i;

	if(param_info)
	{
		for (i=0;i<param_info->num_cols;i++)
		{
			if(param_info->columns[i])
				TDS_ZERO_FREE(param_info->columns[i]);
		}
		if (param_info->num_cols) TDS_ZERO_FREE(param_info->columns);
		if (param_info->current_row) TDS_ZERO_FREE(param_info->current_row);
		TDS_ZERO_FREE(param_info);
	}
}
void tds_free_compute_result(TDSCOMPUTEINFO *comp_info)
{

int i;

	if(comp_info)
	{
		if (comp_info->current_row) 
			TDS_ZERO_FREE(comp_info->current_row);

		if (comp_info->num_cols && comp_info->columns)
		{
			for (i=0;i<comp_info->num_cols;i++)
				if(comp_info->columns[i])
					tds_free_column(comp_info->columns[i]);
			TDS_ZERO_FREE(comp_info->columns);
		}

		if (comp_info->by_cols) TDS_ZERO_FREE(comp_info->bycolumns);

		TDS_ZERO_FREE(comp_info);
	}

}

void tds_free_compute_results(TDSCOMPUTEINFO **comp_info, TDS_INT num_comp)
{

int i;

    for ( i = 0 ; i < num_comp; i++ ) {
      if (comp_info && comp_info[i] )
         tds_free_compute_result(comp_info[i]);
    }
    if (num_comp)
       TDS_ZERO_FREE(comp_info);

}

void tds_free_results(TDSRESULTINFO *res_info)
{
int i;


	if(res_info)
	{
		if (res_info->current_row) 
           TDS_ZERO_FREE(res_info->current_row);

		for (i=0;i<res_info->num_cols;i++)
		{
			if(res_info->columns && res_info->columns[i])
				tds_free_column(res_info->columns[i]);
		}
		if (res_info->num_cols) TDS_ZERO_FREE(res_info->columns);

		TDS_ZERO_FREE(res_info);
	}

}
void tds_free_all_results(TDSSOCKET *tds)
{
	tds_free_results(tds->res_info);
	tds->res_info = NULL;
	tds_free_param_results(tds->param_info);
	tds->param_info = NULL;
	tds_free_compute_results(tds->comp_info, tds->num_comp_info);
	tds->comp_info = NULL;
    tds->num_comp_info = 0;
}
void tds_free_column(TDSCOLINFO *column)
{
	if (column->column_textvalue) TDS_ZERO_FREE(column->column_textvalue);
	TDS_ZERO_FREE(column);
}
TDSCONTEXT *tds_alloc_context(void)
{
TDSCONTEXT *context;
TDSLOCINFO *locale;

	locale = tds_get_locale();
	if (!locale) return NULL;

	context = (TDSCONTEXT *) malloc(sizeof(TDSCONTEXT));
	if (!context) {
		tds_free_locale(locale);
		return NULL;
	}
	memset(context, '\0', sizeof(TDSCONTEXT));
	context->locale = locale;

	return context;
}
void tds_free_context(TDSCONTEXT *context)
{
	if (context->locale) tds_free_locale(context->locale);
	TDS_ZERO_FREE(context);
}
TDSLOCINFO *tds_alloc_locale(void)
{
TDSLOCINFO *locale;

	locale = (TDSLOCINFO *) malloc(sizeof(TDSLOCINFO));
	if (!locale) return NULL;
	memset(locale, '\0', sizeof(TDSLOCINFO));

	return locale;
}
static const unsigned char defaultcaps[] = 
{0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x0A,104,0x00,0x00,0x00};
/**
 * Allocate space for configure structure and initialize with default values
 * @param locale locale information (copied to configuration information)
 * @result allocated structure or NULL if out of memory
 */
TDSCONNECTINFO *tds_alloc_connect(TDSLOCINFO *locale)
{
TDSCONNECTINFO *connect_info;
char hostname[30];
	
	TEST_MALLOC(connect_info,TDSCONNECTINFO);
	memset(connect_info, '\0', sizeof(TDSCONNECTINFO));
	tds_dstr_init(&connect_info->server_name);
	tds_dstr_init(&connect_info->language);
	tds_dstr_init(&connect_info->char_set);
	tds_dstr_init(&connect_info->host_name);
	tds_dstr_init(&connect_info->app_name);
	tds_dstr_init(&connect_info->user_name);
	tds_dstr_init(&connect_info->password);
	tds_dstr_init(&connect_info->library);
	tds_dstr_init(&connect_info->ip_addr);
	tds_dstr_init(&connect_info->database);
	tds_dstr_init(&connect_info->dump_file);
	tds_dstr_init(&connect_info->default_domain);
	tds_dstr_init(&connect_info->client_charset);

	/* fill in all hardcoded defaults */
	if (!tds_dstr_copy(&connect_info->server_name,TDS_DEF_SERVER))
		goto Cleanup;
	connect_info->major_version = TDS_DEF_MAJOR;
	connect_info->minor_version = TDS_DEF_MINOR;
	connect_info->port = TDS_DEF_PORT;
	connect_info->block_size = TDS_DEF_BLKSZ;
	if (locale) {
		if (locale->language) 
			if (!tds_dstr_copy(&connect_info->language,locale->language))
				goto Cleanup;
		if (locale->char_set) 
			if (!tds_dstr_copy(&connect_info->char_set,locale->char_set))
				goto Cleanup;
	}
	if (tds_dstr_isempty(&connect_info->language)) {
		if (!tds_dstr_copy(&connect_info->language,TDS_DEF_LANG))
			goto Cleanup;
	}
	if (tds_dstr_isempty(&connect_info->char_set)) {
		if (!tds_dstr_copy(&connect_info->char_set,TDS_DEF_CHARSET))
			goto Cleanup;
	}
	connect_info->try_server_login = 1;
	memset(hostname,'\0', sizeof(hostname));
	gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname)-1]='\0'; /* make sure it's truncated */
	if (!tds_dstr_copy(&connect_info->host_name,hostname))
		goto Cleanup;
	
	memcpy(connect_info->capabilities,defaultcaps,TDS_MAX_CAPABILITY);
	return connect_info;
Cleanup:
	tds_free_connect(connect_info);
	return NULL;
}
TDSLOGIN *tds_alloc_login(void)
{
TDSLOGIN *tds_login;
char *tdsver;

	tds_login = (TDSLOGIN *) malloc(sizeof(TDSLOGIN));
	if (!tds_login) return NULL;
	memset(tds_login, '\0', sizeof(TDSLOGIN));
	tds_dstr_init(&tds_login->server_name);
	tds_dstr_init(&tds_login->language);
	tds_dstr_init(&tds_login->char_set);
	tds_dstr_init(&tds_login->host_name);
	tds_dstr_init(&tds_login->app_name);
	tds_dstr_init(&tds_login->user_name);
	tds_dstr_init(&tds_login->password);
	tds_dstr_init(&tds_login->library);
	if ((tdsver=getenv("TDSVER"))) {
		if (!strcmp(tdsver,"42") || !strcmp(tdsver,"4.2")) {
			tds_login->major_version=4;
			tds_login->minor_version=2;
		} else if (!strcmp(tdsver,"46") || !strcmp(tdsver,"4.6")) {
			tds_login->major_version=4;
			tds_login->minor_version=6;
		} else if (!strcmp(tdsver,"50") || !strcmp(tdsver,"5.0")) {
			tds_login->major_version=5;
			tds_login->minor_version=0;
		} else if (!strcmp(tdsver,"70") || !strcmp(tdsver,"7.0")) {
			tds_login->major_version=7;
			tds_login->minor_version=0;
		} else if (!strcmp(tdsver,"80") || !strcmp(tdsver,"8.0")) {
			tds_login->major_version=8;
			tds_login->minor_version=0;
		}
		/* else unrecognized...use compile time default above */
	}
	memcpy(tds_login->capabilities,defaultcaps,TDS_MAX_CAPABILITY);
	return tds_login;
}
void tds_free_login(TDSLOGIN *login)
{
	if (login) {
		/* for security reason clear memory */
		tds_dstr_zero(&login->password);
		tds_dstr_free(&login->password);
		tds_dstr_free(&login->server_name);
		tds_dstr_free(&login->language);
		tds_dstr_free(&login->char_set);
		tds_dstr_free(&login->host_name);
		tds_dstr_free(&login->app_name);
		tds_dstr_free(&login->user_name);
		tds_dstr_free(&login->library);
		free(login);
	}
}
TDSSOCKET *tds_alloc_socket(TDSCONTEXT *context, int bufsize)
{
TDSSOCKET *tds_socket;
TDSICONVINFO *iconv_info;

	TEST_MALLOC(tds_socket, TDSSOCKET);
	memset(tds_socket, '\0', sizeof(TDSSOCKET));
	tds_socket->tds_ctx = context;
	tds_socket->in_buf_max=0;
	TEST_MALLOCN(tds_socket->out_buf,unsigned char,bufsize);
	tds_socket->parent = (char*)NULL;
	if (!(tds_socket->env = tds_alloc_env(tds_socket)))
		goto Cleanup;
	TEST_MALLOC(iconv_info,TDSICONVINFO);
	tds_socket->iconv_info = (void *) iconv_info;
	memset(tds_socket->iconv_info,'\0',sizeof(TDSICONVINFO));
#if HAVE_ICONV
	iconv_info->cdfrom = (iconv_t)-1;
	iconv_info->cdto = (iconv_t)-1;
#endif
	/* Jeff's hack, init to no timeout */
	tds_socket->timeout = 0;                
	tds_init_write_buf(tds_socket);
	tds_socket->s = -1;
	return tds_socket;
Cleanup:
	tds_free_socket(tds_socket);
	return NULL;
}
TDSSOCKET *tds_realloc_socket(int bufsize)
{
   return NULL; /* XXX */
}
void tds_free_socket(TDSSOCKET *tds)
{
TDSICONVINFO *iconv_info;

	if (tds) {
		tds_free_all_results(tds);
		tds_free_env(tds);
		tds_free_dynamic(tds);
		if (tds->in_buf) TDS_ZERO_FREE(tds->in_buf);
		if (tds->out_buf) TDS_ZERO_FREE(tds->out_buf);
		tds_close_socket(tds);
		if (tds->date_fmt) free(tds->date_fmt);
		if (tds->iconv_info) {
			iconv_info = (TDSICONVINFO *) tds->iconv_info;
			if (iconv_info->use_iconv) tds_iconv_close(tds);
			free(tds->iconv_info);
		}
		if (tds->date_fmt) free(tds->date_fmt);
		TDS_ZERO_FREE(tds);
	}
}
void tds_free_locale(TDSLOCINFO *locale)
{
	if (locale->language) free(locale->language);
	if (locale->char_set) free(locale->char_set);
	if (locale->date_fmt) free(locale->date_fmt);
	TDS_ZERO_FREE(locale);
}
void tds_free_connect(TDSCONNECTINFO *connect_info)
{
	tds_dstr_free(&connect_info->server_name);
	tds_dstr_free(&connect_info->host_name);
	tds_dstr_free(&connect_info->language);
	tds_dstr_free(&connect_info->char_set);
	tds_dstr_free(&connect_info->ip_addr);
	tds_dstr_free(&connect_info->database);
	tds_dstr_free(&connect_info->dump_file);
	tds_dstr_free(&connect_info->default_domain);
	tds_dstr_free(&connect_info->client_charset);
	tds_dstr_free(&connect_info->app_name);
	tds_dstr_free(&connect_info->user_name);
	/* cleared for security reason */
	tds_dstr_zero(&connect_info->password);
	tds_dstr_free(&connect_info->password);
	tds_dstr_free(&connect_info->library);
	TDS_ZERO_FREE(connect_info);
}

TDSENVINFO *tds_alloc_env(TDSSOCKET *tds)
{
TDSENVINFO *env;

	env = (TDSENVINFO *) malloc(sizeof(TDSENVINFO));
	if (!env) return NULL;
	memset(env,'\0',sizeof(TDSENVINFO));
	env->block_size = TDS_DEF_BLKSZ;

	return env;
}
void tds_free_env(TDSSOCKET *tds)
{
	if (tds->env) {
		if (tds->env->language)
			TDS_ZERO_FREE(tds->env->language);
		if (tds->env->charset)
			TDS_ZERO_FREE(tds->env->charset);
		if (tds->env->database)
			TDS_ZERO_FREE(tds->env->database);
		TDS_ZERO_FREE(tds->env);
	}
}

void
tds_free_msg(TDSMSGINFO *msg_info)
{
	if (msg_info) {
		msg_info->priv_msg_type = 0;
		msg_info->msg_number = 0;
		msg_info->msg_state = 0;
		msg_info->msg_level = 0; 
		msg_info->line_number = 0;  
		if (msg_info->message) TDS_ZERO_FREE(msg_info->message);
		if (msg_info->server) TDS_ZERO_FREE(msg_info->server);
		if (msg_info->proc_name) TDS_ZERO_FREE(msg_info->proc_name);
		if (msg_info->sql_state) TDS_ZERO_FREE(msg_info->sql_state);
	}
}

/** \@} */
