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


static char software_version[] = "$Id: rpc.c,v 1.9 2002-11-23 13:54:39 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static void rpc_clear(DBREMOTE_PROC * rpc);
static void param_clear(DBREMOTE_PROC_PARAM * pparam);

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
	
	switch (options) {
	case DBRPCRECOMPILE:
		break;
	case DBRPCRESET:
		rpc_clear(dbproc->rpc);
		return SUCCEED;
		break; /* OK */
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
	
	rpc->name = (char*) malloc(1 + strlen(rpcname));
	if (rpc->name == NULL) return FAIL; 	
	
	/* store */
	strcpy(rpc->name, rpcname);
	rpc->options = options;
	rpc->param_list = (DBREMOTE_PROC_PARAM*) NULL;

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
		name = (char*) malloc(1 + strlen(paramname));
		if (name == NULL) return FAIL;
		strcpy(name, paramname);
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
	if (   dbproc == NULL
	    || dbproc->rpc == NULL	/* dbrpcinit should allocate pointer */
	    || dbproc->rpc->name == NULL) /* can't be ready without a name */
	{
		return FAIL;  
	}

	/* FIXME do stuff */
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbrpcsend()\n");


	/* free up the memory */
	rpc_clear(dbproc->rpc);

	return SUCCEED;
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
	rpc = (DBREMOTE_PROC*) NULL;
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

