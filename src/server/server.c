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
#include <tds.h>
#include "server.h"

static char  software_version[]   = "$Id: server.c,v 1.5 2002-09-20 21:08:35 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

void
tds_env_change(TDSSOCKET *tds,int type, char *oldvalue, char *newvalue)
{
TDS_SMALLINT totsize;

	tds_put_byte(tds,TDS_ENV_CHG_TOKEN);
	switch(type) {
		/* database */
		case 1:
			/* totsize = type + newlen + newvalue + oldlen + oldvalue  */
			totsize = strlen(oldvalue) + strlen(newvalue) + 3;
			tds_put_smallint(tds, totsize);
			tds_put_byte(tds,type);
			tds_put_byte(tds,strlen(newvalue));
			tds_put_n(tds,newvalue,strlen(newvalue));
			tds_put_byte(tds,strlen(oldvalue));
			tds_put_n(tds,oldvalue,strlen(oldvalue));
			break;
		/* language */
		case 2:
			/* totsize = type + len + string + \0 */
			totsize = strlen(newvalue) + 3;
			tds_put_smallint(tds, totsize);
			tds_put_byte(tds,type);
			tds_put_byte(tds,strlen(newvalue));
			tds_put_n(tds,newvalue,strlen(newvalue));
			tds_put_byte(tds,'\0');
			break;
		/* code page would be 3 */
		/* packet size */
		case 4:
			/* totsize = type + len + string + \0 */
			totsize = strlen(newvalue) + 3;
			tds_put_smallint(tds, totsize);
			tds_put_byte(tds,type);
			tds_put_byte(tds,strlen(newvalue));
			tds_put_n(tds,newvalue,strlen(newvalue));
			tds_put_byte(tds,'\0');
			break;
	}
}

void tds_send_eed(TDSSOCKET *tds,int msgno, int msgstate, int severity,
	char *msgtext, char *srvname, char *procname, int line)
{
int totsize;

	tds_put_byte(tds,TDS_EED_TOKEN);
	totsize = 7 + strlen(procname) + 5 + strlen(msgtext) + 2 + strlen(srvname) + 3;
	tds_put_smallint(tds,totsize);
	tds_put_smallint(tds,msgno);
	tds_put_smallint(tds,0); /* unknown */
	tds_put_byte(tds, msgstate);
	tds_put_byte(tds, severity);
	tds_put_byte(tds,strlen(procname));
	tds_put_n(tds,procname,strlen(procname));
	tds_put_byte(tds,0); /* unknown */
	tds_put_byte(tds,1); /* unknown */
	tds_put_byte(tds,0); /* unknown */
	tds_put_smallint(tds,strlen(msgtext)+1);
	tds_put_n(tds,msgtext,strlen(msgtext));
	tds_put_byte(tds, severity);
	tds_put_byte(tds,strlen(srvname));
	tds_put_n(tds,srvname,strlen(srvname));
	tds_put_byte(tds,0); /* unknown */
	tds_put_byte(tds,1); /* unknown */
	tds_put_byte(tds,0); /* unknown */
}
void tds_send_msg(TDSSOCKET *tds,int msgno, int msgstate, int severity,
	char *msgtext, char *srvname, char *procname, int line)
{
int msgsz;

	tds_put_byte(tds,TDS_MSG_TOKEN);
	msgsz = 4    /* msg no    */
		+ 1       /* msg state */
		+ 1       /* severity  */
		+ strlen(msgtext) + 1       
		+ strlen(srvname) + 1
		+ strlen(procname) + 1
		+ 2;      /* line number */
	tds_put_smallint(tds,msgsz);
	tds_put_int(tds, msgno);
	tds_put_byte(tds, msgstate);
	tds_put_byte(tds, severity);
	tds_put_smallint(tds, strlen(msgtext));
	tds_put_n(tds, msgtext, strlen(msgtext));
	tds_put_byte(tds,strlen(srvname));
	tds_put_n(tds,srvname,strlen(srvname));
	if (procname && strlen(procname)) {
		tds_put_byte(tds,strlen(procname)); 
		tds_put_n(tds,procname,strlen(procname));
	} else {
		tds_put_byte(tds,0); 
	}
	tds_put_smallint(tds,line);
}
void tds_send_err(TDSSOCKET *tds,int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
	tds_put_byte(tds,TDS_ERR_TOKEN);
}
void tds_send_login_ack(TDSSOCKET *tds, char *progname)
{
	tds_put_byte(tds,TDS_LOGIN_ACK_TOKEN);
	tds_put_smallint(tds,10+strlen(progname)); /* length of message */
	if IS_TDS42(tds) {
		tds_put_byte(tds,1);
		tds_put_byte(tds,4);
		tds_put_byte(tds,2);
	} else {
		tds_put_byte(tds,5);
		tds_put_byte(tds,5);
		tds_put_byte(tds,0);
	}
	tds_put_byte(tds,0); /* unknown */
	tds_put_byte(tds,0); /* unknown */
	tds_put_byte(tds,strlen(progname)); 
	tds_put_n(tds,progname,strlen(progname)); 
	tds_put_byte(tds,1); /* unknown */
	tds_put_byte(tds,0); /* unknown */
	tds_put_byte(tds,0); /* unknown */
	tds_put_byte(tds,1); /* unknown */
}
void
tds_send_capabilities_token(TDSSOCKET *tds)
{
	tds_put_byte(tds,TDS_CAP_TOKEN);
	tds_put_byte(tds,18);
	tds_put_byte(tds,0);
	tds_put_byte(tds,1);
	tds_put_byte(tds,7);
	tds_put_byte(tds,7);
	tds_put_byte(tds,97);
	tds_put_byte(tds,65);
	tds_put_byte(tds,207);
	tds_put_byte(tds,255);
	tds_put_byte(tds,255);
	tds_put_byte(tds,230);
	tds_put_byte(tds,2);
	tds_put_byte(tds,7);
	tds_put_byte(tds,0);
	tds_put_byte(tds,0);
	tds_put_byte(tds,2);
	tds_put_byte(tds,0);
	tds_put_byte(tds,0);
	tds_put_byte(tds,0);
	tds_put_byte(tds,0);
}
void
tds_send_253_token(TDSSOCKET *tds, TDS_TINYINT flags, TDS_INT numrows)
{
	tds_put_byte(tds,253);
	tds_put_byte(tds,flags);
	tds_put_byte(tds,0);
	tds_put_byte(tds,2);
	tds_put_byte(tds,0);
	tds_put_int(tds,numrows);
}
void
tds_send_174_token(TDSSOCKET *tds, TDS_SMALLINT numcols)
{
int i;

	tds_put_byte(tds,174);
	tds_put_smallint(tds,numcols);
	for (i=0;i<numcols;i++) {
		tds_put_byte(tds,0);
	}
}
void tds_send_col_name(TDSSOCKET *tds, TDSRESULTINFO *resinfo)
{
int col, hdrsize=0;
TDSCOLINFO *curcol;

	tds_put_byte(tds,TDS_COL_NAME_TOKEN);
	for (col=0;col<resinfo->num_cols;col++) {
		curcol=resinfo->columns[col];
		hdrsize += strlen(curcol->column_name) + 2;
	}

	tds_put_smallint(tds,hdrsize);
	for (col=0;col<resinfo->num_cols;col++) {
		curcol=resinfo->columns[col];
		tds_put_byte(tds, strlen(curcol->column_name));
		/* include the null */
		tds_put_n(tds, curcol->column_name, strlen(curcol->column_name)+1);
	}
}
void tds_send_col_info(TDSSOCKET *tds, TDSRESULTINFO *resinfo)
{
int col, hdrsize=0;
TDSCOLINFO *curcol;

	tds_put_byte(tds,TDS_COL_INFO_TOKEN);

	for (col=0;col<resinfo->num_cols;col++) {
		curcol=resinfo->columns[col];
		hdrsize += 5;
		if (!is_fixed_type(curcol->column_type)) {
			hdrsize++;
		}
	}
	tds_put_smallint(tds,hdrsize);

	for (col=0;col<resinfo->num_cols;col++) {
		curcol=resinfo->columns[col];
		tds_put_n(tds,"\0\0\0\0",4);
		tds_put_byte(tds,curcol->column_type);
		if (!is_fixed_type(curcol->column_type)) {
			tds_put_byte(tds,curcol->column_size);
		}
	}
}

void
tds_send_result(TDSSOCKET *tds, TDSRESULTINFO *resinfo)
{
TDSCOLINFO *curcol;
int i, totlen;

	tds_put_byte(tds,TDS_RESULT_TOKEN);
	totlen = 2;
	for (i=0;i<resinfo->num_cols;i++) {
		curcol = resinfo->columns[i];
		totlen += 8;
		totlen += strlen(curcol->column_name);
		curcol = resinfo->columns[i];
		if (!is_fixed_type(curcol->column_type)) {
			totlen++;
		}
	}
	tds_put_smallint(tds,totlen);
	tds_put_smallint(tds,resinfo->num_cols);
	for (i=0;i<resinfo->num_cols;i++) {
		curcol = resinfo->columns[i];
		tds_put_byte(tds,strlen(curcol->column_name));
		tds_put_n(tds,curcol->column_name,strlen(curcol->column_name));
		tds_put_byte(tds,'0');
		tds_put_smallint(tds, curcol->column_usertype);
		tds_put_smallint(tds,0);
		tds_put_byte(tds, curcol->column_type);
		if (!is_fixed_type(curcol->column_type)) {
			tds_put_byte(tds, curcol->column_size);
		}
		tds_put_byte(tds,0);
	}
}

void
tds_send_row(TDSSOCKET *tds, TDSRESULTINFO *resinfo)
{
TDSCOLINFO *curcol;
int colsize, i;

	tds_put_byte(tds,TDS_ROW_TOKEN);
	for (i=0;i<resinfo->num_cols;i++) {
		curcol = resinfo->columns[i];
		if (!is_fixed_type(curcol->column_type)) {
			/* FIX ME -- I have no way of knowing the actual length of non character variable data (eg nullable int) */
			colsize = strlen((char *)&(resinfo->current_row[curcol->column_offset]));
			tds_put_byte(tds, colsize);
			tds_put_n(tds,&(resinfo->current_row[curcol->column_offset]), colsize);
		} else {
			tds_put_n(tds,&(resinfo->current_row[curcol->column_offset]), get_size_by_type(curcol->column_type));
		}
	}
}
