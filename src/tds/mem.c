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

#include <assert.h>

#include "tds.h"
#include "tdsiconv.h"
#include "tdsstring.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: mem.c,v 1.92 2003-08-15 07:10:41 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version,
	no_unused_var_warn
};

static TDSENVINFO *tds_alloc_env(TDSSOCKET * tds, int bufsize);
static void tds_free_env(TDSSOCKET * tds);

#undef TEST_MALLOC
#define TEST_MALLOC(dest,type) \
	{if (!(dest = (type*)malloc(sizeof(type)))) goto Cleanup;}

#undef TEST_CALLOC
#define TEST_CALLOC(dest,type,n) \
	{if (!(dest = (type*)calloc((n), sizeof(type)))) goto Cleanup;}

#undef TEST_STRDUP
#define TEST_STRDUP(dest,str) \
	{if (!(dest = strdup(str))) goto Cleanup;}

/**
 * \ingroup libtds
 * \defgroup mem Memory allocation
 * Allocate or free resources. Allocation can fail only on out of memory. 
 * In such case they return NULL and leave the state as before call.
 */

/** \addtogroup mem
 *  \@{ 
 */

/** \fn TDSDYNAMIC *tds_alloc_dynamic(TDSSOCKET *tds, const char *id)
 *  \brief Allocate a dynamic statement.
 *  \param tds the connection within which to allocate the statement.
 *  \param id a character label identifying the statement.
 *  \return a pointer to the allocated structure (NULL on failure).
 *
 *  tds_alloc_dynamic is used to implement placeholder code under TDS 5.0
 */
TDSDYNAMIC *
tds_alloc_dynamic(TDSSOCKET * tds, const char *id)
{
	int i;
	TDSDYNAMIC *dyn;
	TDSDYNAMIC **dyns;

	/* check to see if id already exists (shouldn't) */
	for (i = 0; i < tds->num_dyns; i++) {
		if (!strcmp(tds->dyns[i]->id, id)) {
			/* id already exists! just return it */
			return (tds->dyns[i]);
		}
	}

	dyn = (TDSDYNAMIC *) malloc(sizeof(TDSDYNAMIC));
	if (!dyn)
		return NULL;
	memset(dyn, 0, sizeof(TDSDYNAMIC));

	if (!tds->num_dyns) {
		/* if this is the first dynamic stmt */
		dyns = (TDSDYNAMIC **) malloc(sizeof(TDSDYNAMIC *));
	} else {
		/* ok, we have a list and need to add another */
		dyns = (TDSDYNAMIC **) realloc(tds->dyns, sizeof(TDSDYNAMIC *) * (tds->num_dyns + 1));
	}

	if (!dyns) {
		free(dyn);
		return NULL;
	}
	tds->dyns = dyns;
	tds->dyns[tds->num_dyns] = dyn;
	++tds->num_dyns;
	strncpy(dyn->id, id, TDS_MAX_DYNID_LEN);
	dyn->id[TDS_MAX_DYNID_LEN - 1] = '\0';

	return dyn;
}

/** \fn void tds_free_input_params(TDSDYNAMIC *dyn)
 *  \brief Frees all allocated input parameters of a dynamic statement.
 *  \param dyn the dynamic statement whose input parameter are to be freed
 *
 *  tds_free_input_params frees all parameters for the give dynamic statement
 */
void
tds_free_input_params(TDSDYNAMIC * dyn)
{
	TDSPARAMINFO *info;

	info = dyn->params;
	if (info) {
		tds_free_param_results(info);
		dyn->params = NULL;
	}
}

/** \fn void tds_free_dynamic(TDSSOCKET *tds, TDSDYNAMIC *dyn)
 *  \brief Frees dynamic statement and remove from TDS
 *  \param dyn dynamic statement to be freed.
 */
void
tds_free_dynamic(TDSSOCKET * tds, TDSDYNAMIC * dyn)
{
	int i;

	/* avoid pointer to garbage */
	if (tds->cur_dyn == dyn)
		tds->cur_dyn = NULL;

	/* free from tds */
	for (i = 0; i < tds->num_dyns; ++i)
		if (dyn == tds->dyns[i]) {
			tds->dyns[i] = tds->dyns[--tds->num_dyns];
			break;
		}

	tds_free_input_params(dyn);
	free(dyn);
}

/** \fn void tds_free_all_dynamic(TDSSOCKET *tds)
 *  \brief Frees all dynamic statements for a given connection.
 *  \param tds the connection containing the dynamic statements to be freed.
 *
 *  tds_free_all_dynamic frees all dynamic statements for the given TDS socket and
 *  then zeros tds->dyns.
 */
void
tds_free_all_dynamic(TDSSOCKET * tds)
{
	int i;
	TDSDYNAMIC *dyn;

	for (i = 0; i < tds->num_dyns; i++) {
		dyn = tds->dyns[i];
		if (!dyn)
			continue;
		tds_free_input_params(dyn);
		free(dyn);
	}
	if (tds->dyns)
		TDS_ZERO_FREE(tds->dyns);
	tds->num_dyns = 0;
	tds->cur_dyn = NULL;

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
TDSPARAMINFO *
tds_alloc_param_result(TDSPARAMINFO * old_param)
{
	TDSPARAMINFO *param_info;
	TDSCOLINFO *colinfo;
	TDSCOLINFO **cols;

	colinfo = (TDSCOLINFO *) malloc(sizeof(TDSCOLINFO));
	if (!colinfo)
		return NULL;
	memset(colinfo, 0, sizeof(TDSCOLINFO));

	if (!old_param || !old_param->num_cols) {
		cols = (TDSCOLINFO **) malloc(sizeof(TDSCOLINFO *));
	} else {
		cols = (TDSCOLINFO **) realloc(old_param->columns, sizeof(TDSCOLINFO *) * (old_param->num_cols + 1));
	}
	if (!cols)
		goto Cleanup;

	if (!old_param) {
		param_info = (TDSPARAMINFO *) malloc(sizeof(TDSPARAMINFO));
		if (!param_info) {
			free(cols);
			goto Cleanup;
		}
		memset(param_info, '\0', sizeof(TDSPARAMINFO));
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
tds_alloc_param_row(TDSPARAMINFO * info, TDSCOLINFO * curparam)
{
	int null_size, remainder, i;
	TDS_INT row_size;
	unsigned char *row;

	null_size = (unsigned) (info->num_cols + (8 * TDS_ALIGN_SIZE - 1)) / 8u;
	null_size = null_size - null_size % TDS_ALIGN_SIZE;
	null_size -= info->null_info_size;
	if (null_size < 0)
		null_size = 0;

	curparam->column_offset = info->row_size;
	/* the +1 are needed for terminater... still required (freddy77) */
	row_size = info->row_size + curparam->column_size + 1 + null_size;
	remainder = row_size % TDS_ALIGN_SIZE;
	if (remainder)
		row_size += (TDS_ALIGN_SIZE - remainder);

	/* make sure the row buffer is big enough */
	if (info->current_row) {
		row = (unsigned char *) realloc(info->current_row, row_size);
	} else {
		row = (unsigned char *) malloc(row_size);
	}
	if (!row)
		return NULL;
	info->current_row = row;
	info->row_size = row_size;

	/* expand null buffer */
	if (null_size) {
		memmove(row + info->null_info_size + null_size,
			row + info->null_info_size, row_size - null_size - info->null_info_size);
		memset(row + info->null_info_size, 0, null_size);
		info->null_info_size += null_size;
		for (i = 0; i < info->num_cols; ++i) {
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
	int null_sz;

	TEST_MALLOC(info, TDSCOMPUTEINFO);
	memset(info, '\0', sizeof(TDSCOMPUTEINFO));

	TEST_CALLOC(info->columns, TDSCOLINFO *, num_cols);
	memset(info->columns, '\0', sizeof(TDSCOLINFO *) * num_cols);

	tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. point 1\n");
	info->num_cols = num_cols;
	for (col = 0; col < num_cols; col++) {
		TEST_MALLOC(info->columns[col], TDSCOLINFO);
		memset(info->columns[col], '\0', sizeof(TDSCOLINFO));
	}

	tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. point 2\n");

	if (by_cols) {
		TEST_CALLOC(info->bycolumns, TDS_TINYINT, by_cols);
		memset(info->bycolumns, '\0', by_cols);
		tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. point 3\n");
		info->by_cols = by_cols;
	}

	null_sz = (num_cols / 8) + 1;
	if (null_sz % TDS_ALIGN_SIZE)
		null_sz = ((null_sz / TDS_ALIGN_SIZE) + 1) * TDS_ALIGN_SIZE;
	/* set the initial row size to the size of the null info */
	info->row_size = info->null_info_size = null_sz;

	return info;
      Cleanup:
	tds_free_compute_result(info);
	return NULL;
}

TDSCOMPUTEINFO **
tds_alloc_compute_results(TDS_INT * num_comp_results, TDSCOMPUTEINFO ** ci, int num_cols, int by_cols)
{
	int n;
	TDSCOMPUTEINFO **comp_info;
	TDSCOMPUTEINFO *cur_comp_info;

	tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. num_cols = %d bycols = %d\n", num_cols, by_cols);
	tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. num_comp_results = %d\n", *num_comp_results);

	cur_comp_info = tds_alloc_compute_result(num_cols, by_cols);
	if (!cur_comp_info)
		return NULL;

	n = *num_comp_results;
	if (n == 0)
		comp_info = (TDSCOMPUTEINFO **) malloc(sizeof(TDSCOMPUTEINFO *));
	else
		comp_info = (TDSCOMPUTEINFO **) realloc(ci, sizeof(TDSCOMPUTEINFO *) * (n + 1));

	if (!comp_info) {
		tds_free_compute_result(cur_comp_info);
		return NULL;
	}

	comp_info[n] = cur_comp_info;
	*num_comp_results = n + 1;

	tdsdump_log(TDS_DBG_INFO1, "%L alloc_compute_result. num_comp_results = %d\n", *num_comp_results);

	return comp_info;
}

TDSRESULTINFO *
tds_alloc_results(int num_cols)
{
/*TDSCOLINFO *curcol;
 */
	TDSRESULTINFO *res_info;
	int col;
	int null_sz;

	TEST_MALLOC(res_info, TDSRESULTINFO);
	memset(res_info, '\0', sizeof(TDSRESULTINFO));
	TEST_CALLOC(res_info->columns, TDSCOLINFO *, num_cols);
	for (col = 0; col < num_cols; col++) {
		TEST_MALLOC(res_info->columns[col], TDSCOLINFO);
		memset(res_info->columns[col], '\0', sizeof(TDSCOLINFO));
	}
	res_info->num_cols = num_cols;
	null_sz = (num_cols / 8) + 1;
	if (null_sz % TDS_ALIGN_SIZE)
		null_sz = ((null_sz / TDS_ALIGN_SIZE) + 1) * TDS_ALIGN_SIZE;
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
unsigned char *
tds_alloc_row(TDSRESULTINFO * res_info)
{
	unsigned char *ptr;

	ptr = (unsigned char *) malloc(res_info->row_size);
	if (!ptr)
		return NULL;
	memset(ptr, '\0', res_info->row_size);
	return ptr;
}

unsigned char *
tds_alloc_compute_row(TDSCOMPUTEINFO * res_info)
{
	unsigned char *ptr;

	ptr = (unsigned char *) malloc(res_info->row_size);
	if (!ptr)
		return NULL;
	memset(ptr, '\0', res_info->row_size);
	return ptr;
}

void
tds_free_param_results(TDSPARAMINFO * param_info)
{
	tds_free_results(param_info);
}

void
tds_free_compute_result(TDSCOMPUTEINFO * comp_info)
{
	tds_free_results(comp_info);
}

void
tds_free_compute_results(TDSCOMPUTEINFO ** comp_info, TDS_INT num_comp)
{

	int i;

	for (i = 0; i < num_comp; i++) {
		if (comp_info && comp_info[i])
			tds_free_compute_result(comp_info[i]);
	}
	if (num_comp)
		TDS_ZERO_FREE(comp_info);

}

void
tds_free_results(TDSRESULTINFO * res_info)
{
	int i;
	TDSCOLINFO *curcol;

	if (!res_info)
		return;

	if (res_info->num_cols && res_info->columns) {
		for (i = 0; i < res_info->num_cols; i++)
			if ((curcol = res_info->columns[i]) != NULL) {
				if (res_info->current_row && is_blob_type(curcol->column_type)) {
					free(((TDSBLOBINFO *) (res_info->current_row + curcol->column_offset))->textvalue);
				}
				free(curcol);
			}
		free(res_info->columns);
	}

	if (res_info->current_row)
		free(res_info->current_row);

	if (res_info->bycolumns)
		free(res_info->bycolumns);

	free(res_info);
}

void
tds_free_all_results(TDSSOCKET * tds)
{
	tds_free_results(tds->res_info);
	tds->res_info = NULL;
	tds_free_param_results(tds->param_info);
	tds->param_info = NULL;
	tds_free_compute_results(tds->comp_info, tds->num_comp_info);
	tds->comp_info = NULL;
	tds->num_comp_info = 0;
	tds->has_status = 0;
	tds->ret_status = 0;
}

TDSCONTEXT *
tds_alloc_context(void)
{
	TDSCONTEXT *context;
	TDSLOCALE *locale;

	locale = tds_get_locale();
	if (!locale)
		return NULL;

	context = (TDSCONTEXT *) malloc(sizeof(TDSCONTEXT));
	if (!context) {
		tds_free_locale(locale);
		return NULL;
	}
	memset(context, '\0', sizeof(TDSCONTEXT));
	context->locale = locale;

	return context;
}

void
tds_free_context(TDSCONTEXT * context)
{
	if (!context)
		return;

	tds_free_locale(context->locale);
	free(context);
}

TDSLOCALE *
tds_alloc_locale(void)
{
	TDSLOCALE *locale;

	locale = (TDSLOCALE *) malloc(sizeof(TDSLOCALE));
	if (!locale)
		return NULL;
	memset(locale, 0, sizeof(TDSLOCALE));

	return locale;
}
static const unsigned char defaultcaps[] = { 0x01, 0x09, 0x00, 0x00, 0x06, 0x6D, 0x7F, 0xFF, 0xFF, 0xFF, 0xFE,
	0x02, 0x09, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x68, 0x00, 0x00, 0x00
};

/**
 * Allocate space for configure structure and initialize with default values
 * @param locale locale information (copied to configuration information)
 * @result allocated structure or NULL if out of memory
 */
TDSCONNECTINFO *
tds_alloc_connect(TDSLOCALE * locale)
{
	TDSCONNECTINFO *connect_info;
	char hostname[128];

	TEST_MALLOC(connect_info, TDSCONNECTINFO);
	memset(connect_info, '\0', sizeof(TDSCONNECTINFO));
	tds_dstr_init(&connect_info->server_name);
	tds_dstr_init(&connect_info->language);
	tds_dstr_init(&connect_info->server_charset);
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
	if (!tds_dstr_copy(&connect_info->server_name, TDS_DEF_SERVER))
		goto Cleanup;
	connect_info->major_version = TDS_DEF_MAJOR;
	connect_info->minor_version = TDS_DEF_MINOR;
	connect_info->port = TDS_DEF_PORT;
	connect_info->block_size = 0;
	/* TODO use system default ?? */
	if (!tds_dstr_copy(&connect_info->client_charset, "ISO-8859-1"))
		goto Cleanup;
	if (locale) {
		if (locale->language)
			if (!tds_dstr_copy(&connect_info->language, locale->language))
				goto Cleanup;
		if (locale->char_set)
			if (!tds_dstr_copy(&connect_info->server_charset, locale->char_set))
				goto Cleanup;
	}
	if (tds_dstr_isempty(&connect_info->language)) {
		if (!tds_dstr_copy(&connect_info->language, TDS_DEF_LANG))
			goto Cleanup;
	}
	if (tds_dstr_isempty(&connect_info->server_charset)) {
		if (!tds_dstr_copy(&connect_info->server_charset, TDS_DEF_CHARSET))
			goto Cleanup;
	}
	connect_info->try_server_login = 1;
	memset(hostname, '\0', sizeof(hostname));
	gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname) - 1] = '\0';	/* make sure it's truncated */
	if (!tds_dstr_copy(&connect_info->host_name, hostname))
		goto Cleanup;

	memcpy(connect_info->capabilities, defaultcaps, TDS_MAX_CAPABILITY);
	return connect_info;
      Cleanup:
	tds_free_connect(connect_info);
	return NULL;
}

TDSLOGIN *
tds_alloc_login(void)
{
	TDSLOGIN *tds_login;

	tds_login = (TDSLOGIN *) malloc(sizeof(TDSLOGIN));
	if (!tds_login)
		return NULL;
	memset(tds_login, '\0', sizeof(TDSLOGIN));
	tds_dstr_init(&tds_login->server_name);
	tds_dstr_init(&tds_login->language);
	tds_dstr_init(&tds_login->server_charset);
	tds_dstr_init(&tds_login->host_name);
	tds_dstr_init(&tds_login->app_name);
	tds_dstr_init(&tds_login->user_name);
	tds_dstr_init(&tds_login->password);
	tds_dstr_init(&tds_login->library);
	tds_dstr_init(&tds_login->client_charset);
	memcpy(tds_login->capabilities, defaultcaps, TDS_MAX_CAPABILITY);
	return tds_login;
}

void
tds_free_login(TDSLOGIN * login)
{
	if (login) {
		/* for security reason clear memory */
		tds_dstr_zero(&login->password);
		tds_dstr_free(&login->password);
		tds_dstr_free(&login->server_name);
		tds_dstr_free(&login->language);
		tds_dstr_free(&login->server_charset);
		tds_dstr_free(&login->host_name);
		tds_dstr_free(&login->app_name);
		tds_dstr_free(&login->user_name);
		tds_dstr_free(&login->library);
		tds_dstr_free(&login->client_charset);
		free(login);
	}
}
TDSSOCKET *
tds_alloc_socket(TDSCONTEXT * context, int bufsize)
{
	TDSSOCKET *tds_socket;
	TDSICONVINFO *iconv_info;
	int i;

	TEST_MALLOC(tds_socket, TDSSOCKET);
	memset(tds_socket, '\0', sizeof(TDSSOCKET));
	tds_socket->tds_ctx = context;
	tds_socket->in_buf_max = 0;
	TEST_CALLOC(tds_socket->out_buf, unsigned char, bufsize);

	tds_socket->parent = (char *) NULL;
	tds_socket->option_flag2 = 0x03;	/* TDS 7.0: 0x02 indicates ODBC driver; 
						 * 0x01 means change to initial language must succeed */
	if (!(tds_socket->env = tds_alloc_env(tds_socket, bufsize)))
		goto Cleanup;

	TEST_CALLOC(iconv_info, TDSICONVINFO, initial_iconv_info_count);
	tds_socket->iconv_info = iconv_info;
	tds_socket->iconv_info_count = initial_iconv_info_count;

	/* an iconv conversion descriptor of -1 means we don't use iconv. */
	for (i = 0; i < initial_iconv_info_count; ++i) {
		iconv_info[i].to_wire = (iconv_t) - 1;
		iconv_info[i].from_wire = (iconv_t) - 1;
	}

	/* Jeff's hack, init to no timeout */
	tds_socket->timeout = 0;
	tds_init_write_buf(tds_socket);
	tds_socket->s = -1;
	tds_socket->env_chg_func = NULL;
	tds_socket->chkintr = NULL;
	tds_socket->hndlintr = NULL;
	return tds_socket;
      Cleanup:
	tds_free_socket(tds_socket);
	return NULL;
}

TDSSOCKET *
tds_realloc_socket(TDSSOCKET * tds, int bufsize)
{
	unsigned char *new_out_buf;

	assert(tds && tds->env && tds->out_buf);

	if (tds->env->block_size == bufsize)
		return tds;

	if ((new_out_buf = (unsigned char *) realloc(tds->out_buf, bufsize)) != NULL) {
		tds->out_buf = new_out_buf;
		tds->env->block_size = bufsize;
		return tds;
	}
	return NULL;
}

void
tds_free_socket(TDSSOCKET * tds)
{
	if (tds) {
		tds_free_all_results(tds);
		tds_free_env(tds);
		tds_free_all_dynamic(tds);
		if (tds->in_buf)
			TDS_ZERO_FREE(tds->in_buf);
		if (tds->out_buf)
			TDS_ZERO_FREE(tds->out_buf);
		tds_close_socket(tds);
		if (tds->date_fmt)
			free(tds->date_fmt);
		if (tds->iconv_info) {
			tds_iconv_close(tds);
			free(tds->iconv_info);
		}
		if (tds->product_name)
			free(tds->product_name);
		TDS_ZERO_FREE(tds);
	}
}
void
tds_free_locale(TDSLOCALE * locale)
{
	if (!locale)
		return;

	if (locale->language)
		free(locale->language);
	if (locale->char_set)
		free(locale->char_set);
	if (locale->date_fmt)
		free(locale->date_fmt);
	free(locale);
}

void
tds_free_connect(TDSCONNECTINFO * connect_info)
{
	tds_dstr_free(&connect_info->server_name);
	tds_dstr_free(&connect_info->host_name);
	tds_dstr_free(&connect_info->language);
	tds_dstr_free(&connect_info->server_charset);
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

static TDSENVINFO *
tds_alloc_env(TDSSOCKET * tds, int bufsize)
{
	TDSENVINFO *env;

	env = (TDSENVINFO *) malloc(sizeof(TDSENVINFO));
	if (!env)
		return NULL;
	memset(env, '\0', sizeof(TDSENVINFO));
	env->block_size = bufsize;

	return env;
}

static void
tds_free_env(TDSSOCKET * tds)
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
tds_free_msg(TDSMSGINFO * msg_info)
{
	if (msg_info) {
		msg_info->priv_msg_type = 0;
		msg_info->msg_number = 0;
		msg_info->msg_state = 0;
		msg_info->msg_level = 0;
		msg_info->line_number = 0;
		if (msg_info->message)
			TDS_ZERO_FREE(msg_info->message);
		if (msg_info->server)
			TDS_ZERO_FREE(msg_info->server);
		if (msg_info->proc_name)
			TDS_ZERO_FREE(msg_info->proc_name);
		if (msg_info->sql_state)
			TDS_ZERO_FREE(msg_info->sql_state);
	}
}

#define SQLS_ENTRY(number,state) case number: p = state; break
char *
tds_alloc_lookup_sqlstate(TDSSOCKET * tds, int msgnum)
{
	const char *p = NULL;
	char *q = NULL;

	if (TDS_IS_MSSQL(tds)) {
		switch (msgnum) {	/* MSSQL Server */

			SQLS_ENTRY(3621, "01000");
			SQLS_ENTRY(8153, "01003");	/* Null in aggregate */
			SQLS_ENTRY(512, "21000");	/* Subquery returns more than one value */
			SQLS_ENTRY(213, "21S01");	/* Insert column list mismatch */
			SQLS_ENTRY(109, "21S01");
			SQLS_ENTRY(110, "21S01");
			SQLS_ENTRY(1774, "21S02");	/* Ref column mismatch */
			SQLS_ENTRY(8152, "22001");	/* String data would be truncated */
			SQLS_ENTRY(5146, "22003");	/* Numeric value out of range */
			SQLS_ENTRY(220, "22003");	/* Arithmetic overflow */
			SQLS_ENTRY(232, "22003");
			SQLS_ENTRY(3606, "22003");
			SQLS_ENTRY(8115, "22003");
			SQLS_ENTRY(210, "22007");	/* Invalid datetime format */
			SQLS_ENTRY(241, "22007");
			SQLS_ENTRY(295, "22007");
			SQLS_ENTRY(242, "22008");	/* Datetime out of range */
			SQLS_ENTRY(296, "22008");
			SQLS_ENTRY(298, "22008");
			SQLS_ENTRY(535, "22008");
			SQLS_ENTRY(542, "22008");
			SQLS_ENTRY(3607, "22012");	/* Div by zero */
			SQLS_ENTRY(8134, "22012");
			SQLS_ENTRY(245, "22018");	/* Syntax error? */
			SQLS_ENTRY(2627, "23000");	/* Constraint violation */
			SQLS_ENTRY(547, "23000");
			SQLS_ENTRY(550, "23000");
			SQLS_ENTRY(4415, "23000");
			SQLS_ENTRY(1505, "23000");
			SQLS_ENTRY(1508, "23000");
			SQLS_ENTRY(3725, "23000");
			SQLS_ENTRY(3726, "23000");
			SQLS_ENTRY(4712, "23000");
			SQLS_ENTRY(10055, "23000");
			SQLS_ENTRY(10065, "23000");
			SQLS_ENTRY(11011, "23000");
			SQLS_ENTRY(11040, "23000");
			SQLS_ENTRY(16999, "24000");	/* Invalid cursor state */
			SQLS_ENTRY(16905, "24000");
			SQLS_ENTRY(16917, "24000");
			SQLS_ENTRY(16946, "24000");
			SQLS_ENTRY(16950, "24000");
			SQLS_ENTRY(266, "25000");	/* Invalid transaction state */
			SQLS_ENTRY(626, "25000");
			SQLS_ENTRY(627, "25000");
			SQLS_ENTRY(628, "25000");
			SQLS_ENTRY(3902, "25000");
			SQLS_ENTRY(3903, "25000");
			SQLS_ENTRY(3916, "25000");
			SQLS_ENTRY(3918, "25000");
			SQLS_ENTRY(3919, "25000");
			SQLS_ENTRY(3921, "25000");
			SQLS_ENTRY(3922, "25000");
			SQLS_ENTRY(3926, "25000");
			SQLS_ENTRY(7969, "25000");
			SQLS_ENTRY(8506, "25000");
			SQLS_ENTRY(15626, "25000");
			SQLS_ENTRY(18456, "28000");	/* Login failed? */
			SQLS_ENTRY(17308, "42000");	/* Syntax/Access violation */
			SQLS_ENTRY(17571, "42000");
			SQLS_ENTRY(18002, "42000");
			SQLS_ENTRY(229, "42000");
			SQLS_ENTRY(230, "42000");
			SQLS_ENTRY(262, "42000");
			SQLS_ENTRY(2557, "42000");
			SQLS_ENTRY(2571, "42000");
			SQLS_ENTRY(2760, "42000");
			SQLS_ENTRY(3110, "42000");
			SQLS_ENTRY(3704, "42000");
			SQLS_ENTRY(4613, "42000");
			SQLS_ENTRY(4618, "42000");
			SQLS_ENTRY(4834, "42000");
			SQLS_ENTRY(5011, "42000");
			SQLS_ENTRY(5116, "42000");
			SQLS_ENTRY(5812, "42000");
			SQLS_ENTRY(6004, "42000");
			SQLS_ENTRY(6102, "42000");
			SQLS_ENTRY(7956, "42000");
			SQLS_ENTRY(11010, "42000");
			SQLS_ENTRY(11045, "42000");
			SQLS_ENTRY(14126, "42000");
			SQLS_ENTRY(15247, "42000");
			SQLS_ENTRY(15622, "42000");
			SQLS_ENTRY(20604, "42000");
			SQLS_ENTRY(21049, "42000");
			SQLS_ENTRY(113, "42000");
			SQLS_ENTRY(2714, "42S01");	/* Table or view already exists */
			SQLS_ENTRY(208, "42S02");	/* Table or view not found */
			SQLS_ENTRY(1913, "42S11");	/* Index already exists */
			SQLS_ENTRY(15605, "42S11");
			SQLS_ENTRY(307, "42S12");	/* Index not found */
			SQLS_ENTRY(308, "42S12");
			SQLS_ENTRY(10033, "42S12");
			SQLS_ENTRY(15323, "42S12");
			SQLS_ENTRY(18833, "42S12");
			SQLS_ENTRY(4925, "42S21");	/* Column already exists */
			SQLS_ENTRY(21255, "42S21");
			SQLS_ENTRY(1911, "42S22");	/* Column not found */
			SQLS_ENTRY(207, "42S22");
			SQLS_ENTRY(4924, "42S22");
			SQLS_ENTRY(4926, "42S22");
			SQLS_ENTRY(15645, "42S22");
			SQLS_ENTRY(21166, "42S22");
		}
	} else {
		switch (msgnum) {	/* Sybase */
			SQLS_ENTRY(3621, "01000");
			SQLS_ENTRY(9501, "01003");	/* Null in aggregate */
			SQLS_ENTRY(512, "21000");	/* Subquery returns more than one value */
			SQLS_ENTRY(213, "21S01");	/* Insert column list mismatch */
			SQLS_ENTRY(109, "21S01");
			SQLS_ENTRY(110, "21S01");
			SQLS_ENTRY(1715, "21S02");	/* Ref column mismatch */
			SQLS_ENTRY(9502, "22001");	/* String data would be truncated */
			SQLS_ENTRY(220, "22003");	/* Arithmetic overflow */
			SQLS_ENTRY(227, "22003");
			SQLS_ENTRY(232, "22003");
			SQLS_ENTRY(247, "22003");
			SQLS_ENTRY(3606, "22003");
			SQLS_ENTRY(535, "22008");	/* Datetime out of range */
			SQLS_ENTRY(542, "22008");
			SQLS_ENTRY(3607, "22012");	/* Div by zero */
			SQLS_ENTRY(245, "22018");	/* Syntax error? */
			SQLS_ENTRY(544, "23000");	/* Constraint violation */
			SQLS_ENTRY(545, "23000");
			SQLS_ENTRY(546, "23000");
			SQLS_ENTRY(547, "23000");
			SQLS_ENTRY(548, "23000");
			SQLS_ENTRY(549, "23000");
			SQLS_ENTRY(550, "23000");
			SQLS_ENTRY(1505, "23000");
			SQLS_ENTRY(1508, "23000");
			SQLS_ENTRY(565, "24000");	/* Invalid cursor state */
			SQLS_ENTRY(558, "24000");
			SQLS_ENTRY(559, "24000");
			SQLS_ENTRY(6235, "24000");
			SQLS_ENTRY(583, "24000");
			SQLS_ENTRY(6259, "24000");
			SQLS_ENTRY(6260, "24000");
			SQLS_ENTRY(562, "24000");
			SQLS_ENTRY(277, "25000");	/* Invalid transaction state */
			SQLS_ENTRY(611, "25000");
			SQLS_ENTRY(627, "25000");
			SQLS_ENTRY(628, "25000");
			SQLS_ENTRY(641, "25000");
			SQLS_ENTRY(642, "25000");
			SQLS_ENTRY(1276, "25000");
			SQLS_ENTRY(3902, "25000");
			SQLS_ENTRY(3903, "25000");
			SQLS_ENTRY(229, "42000");	/* Syntax/Access violation */
			SQLS_ENTRY(230, "42000");
			SQLS_ENTRY(262, "42000");
			SQLS_ENTRY(4602, "42000");
			SQLS_ENTRY(4603, "42000");
			SQLS_ENTRY(4608, "42000");
			SQLS_ENTRY(10306, "42000");
			SQLS_ENTRY(10323, "42000");
			SQLS_ENTRY(10330, "42000");
			SQLS_ENTRY(10331, "42000");
			SQLS_ENTRY(10332, "42000");
			SQLS_ENTRY(11110, "42000");
			SQLS_ENTRY(11113, "42000");
			SQLS_ENTRY(11118, "42000");
			SQLS_ENTRY(11121, "42000");
			SQLS_ENTRY(17222, "42000");
			SQLS_ENTRY(17223, "42000");
			SQLS_ENTRY(18350, "42000");
			SQLS_ENTRY(18351, "42000");
			SQLS_ENTRY(113, "42000");
			SQLS_ENTRY(2714, "42S01");	/* Table or view already exists */
			SQLS_ENTRY(208, "42S02");	/* Table or view not found */
			SQLS_ENTRY(1913, "42S11");	/* Index already exists */
			SQLS_ENTRY(307, "42S12");	/* Index not found */
			SQLS_ENTRY(7010, "42S12");
			SQLS_ENTRY(18091, "42S12");
			SQLS_ENTRY(1921, "42S21");	/* Column already exists */
			SQLS_ENTRY(1720, "42S22");	/* Column not found */
			SQLS_ENTRY(207, "42S22");
			SQLS_ENTRY(4934, "42S22");
			SQLS_ENTRY(18117, "42S22");
		}
	}

	if (p != NULL && (q = strdup(p)) != NULL) {
		/* FIXME correct here ?? */
		/* Convert known ODBC 3.x states listed above to 2.x */
		if (memcmp(q, "42S", 3) == 0)
			memcpy(q, "S00", 3);

		return q;
	}
	return NULL;
}

/** \@} */
