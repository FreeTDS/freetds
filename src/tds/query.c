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

#include "tds.h"
#include "tdsutil.h"

static char  software_version[]   = "$Id: query.c,v 1.1 2001-10-12 23:29:02 brianb Exp $";
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
		tds_client_msg(tds,10000,7,0,1,
        "Attempt to initiate a new SQL Server operation with results pending.");
		return TDS_FAIL;
	}

	tds_free_all_results(tds);

	tds->rows_affected = 0;
	tds->state = TDS_QUERYING;
	if (IS_TDS50(tds)) {
		bufsize = strlen(query)+6;
		buf = (unsigned char *) malloc(bufsize);
		memset(buf,'\0',bufsize);
		buf[0]=TDS_LANG_TOKEN; 

		bufsize2 = strlen(query) + 1;
		memcpy(buf+1, (void *)&bufsize2, 4);

		memcpy(&buf[6],query,strlen(query));
		tds->out_flag=0x0F;
	} else if (IS_TDS70(tds)) {
		bufsize = strlen(query)*2;
		buf = (unsigned char *) malloc(bufsize);
		memset(buf,'\0',bufsize);
		tds7_ascii2unicode(query, buf, bufsize);
		tds->out_flag=0x01;
	} else { /* 4.2 */
		bufsize = strlen(query);
		buf = (unsigned char *) malloc(bufsize);
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

	if (!IS_TDS50(tds)) {
		tds_client_msg(tds,10000,7,0,1,
        "Dynamic placeholders only supported under TDS 5.0");
		return TDS_FAIL;
	}
	if (tds->state==TDS_PENDING) {
		tds_client_msg(tds,10000,7,0,1,
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
** tds_send_cancel() sends an empty packet (8 byte header only)
** tds_process_cancel should be called directly after this.
*/
int tds_send_cancel(TDSSOCKET *tds)
{
        tds->out_flag=0x06;
        tds_flush_packet(tds);

        return 0;
}

