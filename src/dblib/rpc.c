/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2002	    James K. Lowden
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

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <assert.h>

#include "tds.h"
#include "sybfront.h"
#include "sybdb.h"
#include "dblib.h"
#include <assert.h>


static char software_version[] = "$Id: rpc.c,v 1.12 2002-11-25 10:45:18 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void rpc_clear(DBREMOTE_PROC * rpc);
static void param_clear(DBREMOTE_PROC_PARAM * pparam);

static TDSPARAMINFO* param_info_alloc(DBREMOTE_PROC * rpc);

/**
 * Initialize a remote procedure call. 
 *
 * Only supported option would be DBRPCRECOMPILE, 
 * which causes the stored procedure to be recompiled before executing.
 * FIXME: I don't know the value for DBRPCRECOMPILE and have not added it to sybdb.h
 */
 
RETCODE
dbrpcinit(DBPROCESS * dbproc, char *rpcname, DBSMALLINT options)
{
	DBREMOTE_PROC* rpc;
	
	/* sanity */
	if (dbproc == NULL || rpcname == NULL) return FAIL;
	
	/* FIXME this is a bit field */
	switch (options) {
	case DBRPCRECOMPILE:
		break;
	case DBRPCRESET:
		rpc_clear(dbproc->rpc);
		dbproc->rpc = NULL;
		return SUCCEED;
		break; /* OK */
	case 0:
		break;
	default:
		return FAIL;  
	}
	
	/* to allocate, first find a free node */
	for (rpc = dbproc->rpc; rpc != NULL; rpc = rpc->next) {
		/* check existing nodes for name match (there shouldn't be one) */
		if (!rpc->name)	return FAIL;
		if (strcmp(rpc->name, rpcname) == 0)
			return FAIL 	/* dbrpcsend should free pointer */;	
	}
		
	/* allocate */
	rpc = (DBREMOTE_PROC*) malloc(sizeof(DBREMOTE_PROC));
	if (rpc == NULL) return FAIL;
	memset(rpc, 0, sizeof(DBREMOTE_PROC));
	
	rpc->name = strdup(rpcname);
	if (rpc->name == NULL) return FAIL; 	
	
	/* store */
	rpc->options = options & DBRPCRECOMPILE;
	rpc->param_list = (DBREMOTE_PROC_PARAM*) NULL;
	dbproc->rpc = rpc;

	/* completed */
	tdsdump_log(TDS_DBG_INFO1, "%L dbrpcinit() added rpcname \"%s\"\n",rpcname);
		
	return SUCCEED;
}

/**
 * Add a parameter to a remote procedure call. 
 */
RETCODE
dbrpcparam(DBPROCESS * dbproc, char *paramname, BYTE status, int type, DBINT maxlen, DBINT datalen, BYTE * value)
{
	char *name = NULL;
	DBREMOTE_PROC *rpc;
	DBREMOTE_PROC_PARAM *p;
	DBREMOTE_PROC_PARAM *pparam;

	/* sanity */
	if (dbproc == NULL || value == NULL) return FAIL;
	if (dbproc->rpc == NULL ) return FAIL;

	/* allocate */
	pparam = (DBREMOTE_PROC_PARAM*) malloc(sizeof(DBREMOTE_PROC_PARAM));
	if (pparam == NULL) return FAIL;

	if(paramname) {
		name = strdup(paramname);
		if (name == NULL) return FAIL;
	}
	
	/* initialize */
	pparam->next = (DBREMOTE_PROC_PARAM*) NULL; /* NULL signifies end of linked list */
	pparam->name = name;
	pparam->status = status;
	pparam->type = type;
	pparam->maxlen = maxlen;
	pparam->datalen = datalen;
	pparam->value = value;
	
	/*
	 * traverse the parameter linked list until the end (using the "next" member)
	 */
	for (rpc = dbproc->rpc; rpc->next != NULL; rpc = rpc->next) /* find "current" procedure */
		;
	for (p = rpc->param_list; p != NULL; p = p->next)
		;
		
	/* add to the end of the list */
	p = pparam;
	
	tdsdump_log(TDS_DBG_INFO1, "%L dbrpcparam() added parameter \"%s\"\n",(paramname)? paramname : "");
	
	return SUCCEED;
}

/**
 * Execute the procedure and free associated memory
 */
RETCODE
dbrpcsend(DBPROCESS * dbproc)
{
	DBREMOTE_PROC * rpc;
	
	/* sanity */
	if (   dbproc == NULL
	    || dbproc->rpc == NULL	/* dbrpcinit should allocate pointer */
	    || dbproc->rpc->name == NULL) /* can't be ready without a name */
	{
		return FAIL;  
	}

	/* FIXME do stuff */
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbrpcsend()\n");

	for (rpc = dbproc->rpc; rpc != NULL; rpc = rpc->next) {
		int erc;
		TDSPARAMINFO* pparam_info = param_info_alloc(rpc);
		
		erc = tds_submit_rpc(dbproc->tds_socket, dbproc->rpc->name, pparam_info);
			;
		/* TODO free parameters */
		if (erc == TDS_FAIL) {
			return FAIL;
		}
	}

	/* free up the memory */
	rpc_clear(dbproc->rpc);
	dbproc->rpc = NULL;

	return SUCCEED;
}

/** 
 * Tell the TDSPARAMINFO structure where the data go.  This is a kind of "bind" operation.
 */
static const unsigned char *
param_row_alloc(TDSPARAMINFO *params, TDSCOLINFO *curcol, void *value, int size)
{
	const unsigned char *row = tds_alloc_param_row(params, curcol);
	if (!row) return NULL;
	memcpy(&params->current_row[curcol->column_offset], value, size);
	
	return row;
}

/** 
 * Allocate memory and copy the rpc information into a TDSPARAMINFO structure.
 */
static TDSPARAMINFO*
param_info_alloc(DBREMOTE_PROC * rpc)
{
	int i;
	DBREMOTE_PROC_PARAM *p;
	TDSCOLINFO *pcol;
	TDSPARAMINFO *params=NULL;

	/* sanity */
	if (rpc == NULL) return NULL;
	
	/* see v 1.10 2002/11/23 for first broken attempt */
	
	for (i=0, p = rpc->param_list; p != NULL; p = p->next, i++) {
		const unsigned char * prow;
		
		if (!(params = tds_alloc_param_result(params))) {
			fprintf(stderr, "out of rpc memory!");
			return 0;
		}
		
		pcol = params->columns[i];
		
		/* meta data */
		if (p->name) 
			strncpy (pcol->column_name, p->name, sizeof(pcol->column_name));
		tds_set_column_type(pcol, p->type);
		pcol->column_size		= p->maxlen;
		pcol->column_writeable		= p->status;	/* FIXME not sure what to do here */

		/* actual data */
		pcol->column_cur_size	= p->datalen;
		prow = param_row_alloc(params, pcol, p->value, p->datalen);
		
		if (!prow) {
			fprintf(stderr, "out of memory for rpc row!");
			return 0;
		}
		
	}
	
	return params;
	
}

/**
 * recursively erase the procedure list
 */
static void
rpc_clear(DBREMOTE_PROC * rpc)
{
	if (rpc == NULL) return;
	
	if (rpc->next) {
		rpc_clear(rpc->next);
	}
	
	param_clear(rpc->param_list);
	
	assert(rpc->name);
	free(rpc->name);
	
	free(rpc);
}

/**
 * recursively erase the parameter list
 */
static void
param_clear(DBREMOTE_PROC_PARAM * pparam)
{
	if (pparam == NULL) return;
	
	if (pparam->next) {
		param_clear(pparam->next);
	}
	
	/* free self after clearing children */
	free(pparam);
	pparam = (DBREMOTE_PROC_PARAM*) NULL;
}

