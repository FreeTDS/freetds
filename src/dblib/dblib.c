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
#include "tdsutil.h"
#include "tds.h"
#include "sybfront.h"
#include "sybdb.h"
#include "dblib.h"
#include "tdsconvert.h"
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <stdarg.h>

static char  software_version[]   = "$Id: dblib.c,v 1.22 2002-07-10 05:06:41 jklowden Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


static int _db_get_server_type(int bindtype);
static int _get_printable_size(TDSCOLINFO *colinfo);

static void _set_null_value(DBPROCESS *dbproc, BYTE *varaddr, int datatype, int maxlen);

/* info/err message handler functions (or rather pointers to them) */
int (*g_dblib_msg_handler)() = NULL;
int (*g_dblib_err_handler)() = NULL;
TDSCONTEXT *g_tds_context = NULL;
extern int (*g_tds_msg_handler)(void*);
extern int (*g_tds_err_handler)(void*);
#ifdef TDS42
int g_dblib_version = DBVERSION_42;
#endif
#ifdef TDS50
int g_dblib_version = DBVERSION_100;
#endif
#ifdef TDS46
int g_dblib_version = DBVERSION_46;
#endif
/* I'm taking some liberties here, there is no such thing as DBVERSION_70 in 
** the real world, so we make it up as we go along */
#ifdef TDS70
int g_dblib_version = DBVERSION_70;
#endif

static void buffer_init(DBPROC_ROWBUF *buf)
{
   memset(buf, 0xad, sizeof(*buf));

   buf->buffering_on     = 0;
   buf->first_in_buf     = 0;
   buf->newest           = -1;
   buf->oldest           = 0;
   buf->elcount          = 0;
   buf->element_size     = 0;
   buf->rows_in_buf      = 0;
   buf->rows             = NULL;
   buf->next_row         = 1;
} /* buffer_init()  */

static void buffer_clear(DBPROC_ROWBUF *buf)
{
   buf->next_row         = 1;
   buf->first_in_buf     = 0;
   buf->newest           = -1;
   buf->oldest           = 0;
   buf->rows_in_buf      = 0;
   if (buf->rows) {
      free(buf->rows);
   }
   buf->rows             = NULL;
} /* buffer_clear()  */


static void buffer_free(DBPROC_ROWBUF *buf)
{
   if (buf->rows != NULL)
   {
      free(buf->rows);
   }
   buf->rows = NULL;
} /* clear_buffer()  */

static void buffer_delete_rows(
   DBPROC_ROWBUF *buf,    /* (U) buffer to clear             */
   int            count)  /* (I) number of elements to purge */
{
   assert(count <= buf->elcount); /* possibly a little to pedantic */

   if (count > buf->rows_in_buf)
   {
      count = buf->rows_in_buf;
   }

   
   buf->oldest        = (buf->oldest + count) % buf->elcount;
   buf->rows_in_buf  -= count;
   buf->first_in_buf  = count==buf->rows_in_buf ? buf->next_row-1 : buf->first_in_buf + count;

   
   assert(buf->first_in_buf >= 0);
} /* buffer_delete_rows() */


static int buffer_start_resultset(
   DBPROC_ROWBUF *buf,          /* (U) buffer to clear */
   int            element_size) /*                     */
{
   int    space_needed = -1;

   assert(element_size > 0);

   if (buf->rows != NULL)
   {
      memset(buf->rows, 0xad, buf->element_size*buf->rows_in_buf);
      free(buf->rows);
   }

   buf->first_in_buf     = 0;
   buf->next_row         = 1;
   buf->newest           = -1;
   buf->oldest           = 0;
   buf->elcount          = buf->buffering_on ? buf->elcount : 1;
   buf->element_size     = element_size;
   buf->rows_in_buf      = 0;
   space_needed          = element_size * buf->elcount;   
   buf->rows             = malloc(space_needed);

   return buf->rows==NULL ? FAIL : SUCCEED;
} /* buffer_start_resultset()  */


static void buffer_delete_all_rows(
   DBPROC_ROWBUF *buf)    /* (U) buffer to clear */
{
   buffer_delete_rows(buf, buf->rows_in_buf);
} /* delete_all_buffer_rows() */

static int buffer_index_of_resultset_row(
   DBPROC_ROWBUF *buf,         /* (U) buffer to clear                 */
   int            row_number)  /* (I)                                 */
{
   int   result = -1;

   if (row_number < buf->first_in_buf)
   {
      result = -1;
   }
   else if (row_number > ((buf->rows_in_buf + buf->first_in_buf) -1))
   {
      result = -1;
   }
   else
   {
      result = ((row_number - buf->first_in_buf) 
                + buf->oldest) % buf->elcount;
   }
   return result;
} /* buffer_index_of_resultset_row()  */


static void *buffer_row_address(
   DBPROC_ROWBUF *buf,    /* (U) buffer to clear                 */
   int            index)  /* (I) raw index of row to return      */
{
   void   *result = NULL;

   assert(index >= 0);
   assert(index < buf->elcount);

   if (index>=buf->elcount || index<0)
   {
      result = NULL;
   }
   else
   {
      int   offset = buf->element_size * (index % buf->elcount);
      result = (char *)buf->rows + offset;
   }
   return result;
} /* buffer_row_address()  */


static void buffer_add_row(
   DBPROC_ROWBUF *buf,       /* (U) buffer to add row into  */
   void          *row,       /* (I) pointer to the row data */
   int            row_size)
{
   void   *dest = NULL;

   assert(row_size > 0);
   assert(row_size == buf->element_size);
   
   assert(buf->elcount >= 1);
   
   buf->newest = (buf->newest + 1) % buf->elcount;
   if (buf->rows_in_buf==0 && buf->first_in_buf==0)
   {
      buf->first_in_buf = 1;
   }
   buf->rows_in_buf++;

   /* 
    * if we have wrapped around we need to adjust oldest
    * and rows_in_buf
    */
   if (buf->rows_in_buf > buf->elcount)
   {
      buf->oldest = (buf->oldest + 1) % buf->elcount;
      buf->first_in_buf++;
      buf->rows_in_buf--;
   }
   
   assert(buf->elcount >= buf->rows_in_buf);
   assert( buf->rows_in_buf==0
           || (((buf->oldest+buf->rows_in_buf) - 1)%buf->elcount)==buf->newest);
   assert(buf->rows_in_buf>0 || (buf->first_in_buf==buf->next_row-1));
   assert(buf->rows_in_buf==0 || 
          (buf->first_in_buf<=buf->next_row));
   assert(buf->next_row-1 <= (buf->first_in_buf + buf->rows_in_buf));

   dest = buffer_row_address(buf, buf->newest);
   memcpy(dest, row, row_size);
} /* buffer_add_row()  */


static int buffer_is_full(DBPROC_ROWBUF *buf)
{
   return buf->rows_in_buf==buf->elcount;
} /* buffer_is_full()  */

static void buffer_set_buffering(
   DBPROC_ROWBUF *buf,      /* (U)                                         */
   int            buf_size) /* (I) number of rows to buffer, 0 to turn off */
{
   /* XXX If the user calls this routine in the middle of 
    * a result set and changes the size of the buffering
    * they are pretty much toast.
    *
    * We need to figure out what to do if the user shrinks the 
    * size of the row buffer.  What rows should be thrown out, 
    * what should happen to the current row, etc?
    */

   assert(buf_size >= 0);

   if (buf_size < 0)
   {
      /* XXX is it okay to ignore this? */
   }
   else if (buf_size == 0)
   {
      buf->buffering_on = 0;
      buf->elcount = 1;
      buffer_delete_all_rows(buf);
   }
   else
   {
      buf->buffering_on = 1;
      buffer_clear(buf);
      buffer_free(buf);
      buf->elcount = buf_size;
      if (buf->element_size > 0)
      {
         buf->rows = malloc(buf->element_size * buf->elcount);
      }
      else
      {
         buf->rows = NULL;
      }
   }
} /* buffer_set_buffering()  */

static void buffer_transfer_bound_data(
   DBPROC_ROWBUF *buf,      /* (U)                                         */
   DBPROCESS     *dbproc,   /* (I)                                         */
   int            row_num)  /* (I) resultset row number                    */
{
int            i, j;
TDSCOLINFO    *curcol;
TDSRESULTINFO *resinfo;
TDSSOCKET     *tds;
int            srctype;
BYTE          *src;
int            desttype;
int            destlen;
/* this should probably go somewhere else */
TDS_NUMERIC	numeric;
   
	tds     = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
   
	for (i=0;i<resinfo->num_cols;i++) {
		curcol = resinfo->columns[i];
		if (curcol->column_nullbind) {
			if (tds_get_null(resinfo->current_row,i)) {
				*((DBINT *)curcol->column_nullbind)=-1;
			} else {
				*((DBINT *)curcol->column_nullbind)=0;
			}
		}
		if (curcol->varaddr) {
 			DBINT srclen = -1;
			int   index = buffer_index_of_resultset_row(buf, row_num);
         
			assert (index >= 0);
				/* XXX now what? */
				
			if (is_blob_type(curcol->column_type)) {
				srclen =  curcol->column_textsize;
				src = (BYTE *)curcol->column_textvalue;
			} else {
				src = ((BYTE *)buffer_row_address(buf, index)) 
					+ curcol->column_offset;
			}
			desttype = _db_get_server_type(curcol->column_bindtype);
			srctype = tds_get_conversion_type(curcol->column_type,
				curcol->column_size);

			if (tds_get_null(resinfo->current_row,i)) {
				_set_null_value(dbproc, curcol->varaddr, desttype, 
					curcol->column_bindlen);
	  		} else {

            	if (curcol->column_bindtype == STRINGBIND)
	               destlen = -2;
	            else if (curcol->column_bindtype == NTBSTRINGBIND)
	                    destlen = -1;
	                 else
	                    destlen = curcol->column_bindlen;

        		dbconvert(dbproc,
					      srctype,			/* srctype  */
					      src,			/* src      */
					      srclen,			/* srclen   */
					      desttype,			/* desttype */
					      (BYTE *)curcol->varaddr,	/* dest     */
					      destlen);       	/* destlen  */
			} /* else not null */
		}
	}
} /* buffer_transfer_bound_data()  */

RETCODE dbinit()
{
	/* set the functions in the TDS layer to point to the correct info/err
	* message handler functions */
	g_tds_msg_handler = dblib_handle_info_message;
	g_tds_err_handler = dblib_handle_err_message;

	/* TDSCONTEXT stores a list of current connections so they may be closed
	** with dbexit() */
	g_tds_context = (TDSCONTEXT *) malloc(sizeof(TDSCONTEXT));
	memset(g_tds_context,'\0',sizeof(TDSCONTEXT));

	g_tds_context->locale = tds_get_locale(g_tds_context->locale);
	if( g_tds_context->locale && !g_tds_context->locale->date_fmt ) {
		/* set default in case there's no locale file */
		g_tds_context->locale->date_fmt = "%b %e %Y %l:%M:%S:%z%p"; 
	}

	return SUCCEED;
}
LOGINREC *dblogin()
{
LOGINREC * loginrec;

	loginrec = (LOGINREC *) malloc(sizeof(LOGINREC));
	loginrec->tds_login = (void *) tds_alloc_login();

	/* set default values for loginrec */
	tds_set_library(loginrec->tds_login,"DB-Library");
	/* tds_set_charset(loginrec->tds_login,"iso_1"); */
	/* tds_set_packet(loginrec->tds_login,TDS_DEF_BLKSZ); */

	return loginrec;
}
void dbloginfree(LOGINREC *login)
{
	if (login) {
		tds_free_login(login->tds_login);
		free(login);
	}
}
RETCODE DBSETLCHARSET(LOGINREC *login, char *charset)
{
	tds_set_charset(login->tds_login,charset);
	return SUCCEED;
}
RETCODE DBSETLPACKET(LOGINREC *login, short packet_size)
{
	tds_set_packet(login->tds_login,packet_size);
	return SUCCEED;
}
RETCODE DBSETLPWD(LOGINREC *login, char *password)
{
	tds_set_passwd(login->tds_login,password);
	return SUCCEED;
}
RETCODE DBSETLUSER(LOGINREC *login, char *username)
{
	tds_set_user(login->tds_login,username);
	return SUCCEED;
}
RETCODE DBSETLHOST(LOGINREC *login, char *hostname)
{
	tds_set_host(login->tds_login,hostname);
	return SUCCEED;
}
RETCODE DBSETLAPP(LOGINREC *login, char *application)
{
	tds_set_app(login->tds_login,application);
	return SUCCEED;
}
DBPROCESS *tdsdbopen(LOGINREC *login,char *server)
{
DBPROCESS *dbproc;
char	*envbuf;
int version_value;
   
	dbproc = (DBPROCESS *) malloc(sizeof(DBPROCESS));
	memset(dbproc,'\0',sizeof(DBPROCESS));
	dbproc->avail_flag = TRUE;
	
	tds_set_server(login->tds_login,server);
   
   dbproc->tds_socket = (void *) tds_connect(login->tds_login, g_tds_context->locale, (void *)dbproc);
   dbproc->dbbuf = NULL;
   dbproc->dbbufsz = 0;

   if(dbproc->tds_socket) {
      /* tds_set_parent( dbproc->tds_socket, dbproc); */
      tds_add_connection(g_tds_context, dbproc->tds_socket);
   } else {
      fprintf(stderr,"DB-Library: Login incorrect.\n");
      free(dbproc); /* memory leak fix (mlilback, 11/17/01) */
      return NULL;
   }
   
   buffer_init(&(dbproc->row_buf));
   
   return dbproc;
}

RETCODE dbfcmd(DBPROCESS *dbproc, char *fmt, ...)
{
va_list ap;
char *tmpstr;
int n, size = 1024;
int done = 0;
RETCODE ret;

        tmpstr = (char *)malloc (size);
	if (!tmpstr) return FAIL;

	dbproc->avail_flag = FALSE;

	while (1) {
		va_start(ap, fmt);
		n = vsnprintf (tmpstr, size, fmt, ap);
		va_end(ap);
		if (n > size) { 
			size = n+1; 
		} else if (n<0) {
			size *= 2;  
		} else {
		      /* we had enough space */
                       break;
		}
		tmpstr = realloc (tmpstr, size);
		if (!tmpstr) return FAIL;
	} 

        ret = dbcmd(dbproc, tmpstr);
	free(tmpstr);

	return ret;
}

RETCODE	dbcmd(DBPROCESS *dbproc, char *cmdstring)
{
int newsz;
void *p;

	if(dbproc == NULL) {
		return FAIL;
	}
	dbproc->avail_flag = FALSE;

	if(dbproc->dbbufsz == 0) {
		dbproc->dbbuf = (unsigned char *) malloc(strlen(cmdstring)+1);
		if(dbproc->dbbuf == NULL) {
			return FAIL;
		}
		strcpy((char *)dbproc->dbbuf, cmdstring);
		dbproc->dbbufsz = strlen(cmdstring) + 1;
	} else {
		newsz = strlen(cmdstring) + dbproc->dbbufsz;
		if((p=realloc(dbproc->dbbuf,newsz)) == NULL) {
				return FAIL;
		}
		dbproc->dbbuf = (unsigned char *)p;
		strcat((char *)dbproc->dbbuf, cmdstring);
		dbproc->dbbufsz = newsz;
	}

	return SUCCEED;
}
RETCODE dbsqlexec(DBPROCESS *dbproc)
{
RETCODE   rc = FAIL;
TDSSOCKET *tds;

   if (dbproc == NULL) {
      return FAIL;
   }
   tds = (TDSSOCKET *) dbproc->tds_socket;
   if (!tds || !tds->s) return FAIL;

   if (tds->res_info && tds->res_info->more_results) 
   /* if (dbproc->more_results && tds_is_end_of_results(dbproc->tds_socket)) */
   {
      dbresults(dbproc);
   }
      
   if (SUCCEED == (rc = dbsqlsend(dbproc)))
   {
      /* 
       * XXX We need to observe the timeout value and abort 
       * if this times out.
       */
      rc = dbsqlok(dbproc);
   }
   dbproc->empty_res_hack = 0;
   return rc;
}

RETCODE dbuse(DBPROCESS *dbproc,char *dbname)
{
char query[255];
/* int retval; */

   if (dbproc == NULL) return FAIL;
   sprintf(query,"use %s",dbname);
   dbcmd(dbproc,query);
   dbsqlexec(dbproc);
   while (dbresults(dbproc)!=NO_MORE_RESULTS)
      while (dbnextrow(dbproc)!=NO_MORE_ROWS);
   return SUCCEED;
}

void
dbclose(DBPROCESS *dbproc)
{
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;
int i;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	if (tds) {
                buffer_free(&(dbproc->row_buf));
		tds_free_socket(tds);
	}
	if (dbproc->bcp_tablename)
		free(dbproc->bcp_tablename);
	if (dbproc->bcp_hostfile)
		free(dbproc->bcp_hostfile);
	if (dbproc->bcp_errorfile)
		free(dbproc->bcp_errorfile);
	if (dbproc->bcp_columns) {
		for (i=0;i<dbproc->bcp_colcount;i++) {
			if (dbproc->bcp_columns[i]->data)
				free(dbproc->bcp_columns[i]->data);
			free(dbproc->bcp_columns[i]);
		}
		free(dbproc->bcp_columns);
	}
	if (dbproc->host_columns) {
		for (i=0;i<dbproc->host_colcount;i++) {
			if (dbproc->host_columns[i]->terminator)
				free(dbproc->host_columns[i]->terminator);
			free(dbproc->host_columns[i]);
		}
		free(dbproc->host_columns);
	}

   	dbfreebuf(dbproc);
        tds_del_connection(g_tds_context, dbproc->tds_socket);
	free(dbproc);

        return;
}

void
dbexit()
{
TDSSOCKET *tds;
DBPROCESS *dbproc;
int i;

	/* FIX ME -- this breaks if ctlib/dblib used in same process */
	for (i=0;i<TDS_MAX_CONN;i++) {
		tds = g_tds_context->connection_list[i];
		if (tds) {
			dbproc = (DBPROCESS *) tds->parent;
			dbclose(dbproc);
		}
	}
	tds_free_locale(g_tds_context->locale);
}


RETCODE dbresults_r(DBPROCESS *dbproc, int recursive)
{
RETCODE       retcode = FAIL;
unsigned char marker; 
TDSSOCKET *tds;

   
   /* 
    * For now let's assume we have only 5 possible classes of tokens 
    * at the next byte in the TDS stream
    *   1) The start of a result set, either TDS_RESULT_TOKEN (tds ver 5.0)
    *      or TDS_COL_NAME_TOKEN (tds ver 4.2).
    *   2) A row token (TDS_ROW_TOKEN)
    *   3) An end token (either 0xFD or 0xFE)
    *   4) A done in proc token (0xFF)
    *   5) A message or error token
    */

   if (dbproc == NULL) return FAIL;
   buffer_clear(&(dbproc->row_buf));

   tds = dbproc->tds_socket;
   if (!tds || !tds->s) return FAIL;

   retcode = tds_process_result_tokens(tds);

   if (retcode == TDS_NO_MORE_RESULTS) {
	if (tds->res_info && tds->res_info->rows_exist) {
		return NO_MORE_RESULTS;
	} else {
		if (!dbproc->empty_res_hack) {
			dbproc->empty_res_hack = 1;
			return SUCCEED;
		} else {
			dbproc->empty_res_hack = 0;
			return NO_MORE_RESULTS;
		}
	}
   }
   if (retcode == TDS_SUCCEED) {
   	retcode = buffer_start_resultset(&(dbproc->row_buf), 
                                      tds->res_info->row_size);
   }
   return retcode;
}
   
/* =============================== dbresults() ===============================
 * 
 * Def: 
 * 
 * Ret:  SUCCEED, FAIL, NO_MORE_RESULTS, or NO_MORE_RPC_RESULTS
 * 
 * ===========================================================================
 */
RETCODE dbresults(DBPROCESS *dbproc)
{
RETCODE rc;
   tdsdump_log(TDS_DBG_FUNC, "%L inside dbresults()\n");
   if (dbproc == NULL) return FAIL;
   rc = dbresults_r(dbproc, 0);
   tdsdump_log(TDS_DBG_FUNC, "%L leaving dbresults() returning %d\n",rc);
   return rc;
}


int dbnumcols(DBPROCESS *dbproc)
{
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo)
		return resinfo->num_cols;
	return 0;
}
char *dbcolname(DBPROCESS *dbproc, int column)
{
static char buf[255];
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column < 1 || column > resinfo->num_cols) return NULL;
	strcpy (buf,resinfo->columns[column-1]->column_name);
	return buf;
}

RETCODE dbgetrow(
   DBPROCESS *dbproc, 
   DBINT row)
{
   RETCODE   result = FAIL;
   int       index = buffer_index_of_resultset_row(&(dbproc->row_buf), row);
   if (-1 == index)
   {
      result = NO_MORE_ROWS;
   }
   else
   {
      dbproc->row_buf.next_row = row;
      buffer_transfer_bound_data(&(dbproc->row_buf), dbproc, row);
      dbproc->row_buf.next_row++;
      result = REG_ROW;
   }

   return result;
}

RETCODE dbnextrow(DBPROCESS *dbproc)
{
   TDSRESULTINFO *resinfo;
   TDSSOCKET     *tds;
   int            rc;
   RETCODE        result = FAIL;
   int            marker;
   
   tdsdump_log(TDS_DBG_FUNC, "%L inside dbnextrow()\n");

   if (dbproc == NULL) return FAIL;
   tds = (TDSSOCKET *) dbproc->tds_socket;
   if (!tds || !tds->s) {
      tdsdump_log(TDS_DBG_FUNC, "%L leaving dbnextrow() returning %d\n",FAIL);
      return FAIL;
   }

   resinfo = tds->res_info;
   if (!resinfo) {
      tdsdump_log(TDS_DBG_FUNC, "%L leaving dbnextrow() returning %d\n",NO_MORE_ROWS);
      return NO_MORE_ROWS;
   }

   if (dbproc->row_buf.buffering_on && buffer_is_full(&(dbproc->row_buf))
       && (-1 == buffer_index_of_resultset_row(&(dbproc->row_buf), 
                                               dbproc->row_buf.next_row)))
   {
      result = BUF_FULL;
   }
   else 
   {
      /*
       * Now try to get the dbproc->row_buf.next_row item into the row
       * buffer
       */
      if (-1 != buffer_index_of_resultset_row(&(dbproc->row_buf), 
                                              dbproc->row_buf.next_row))
      {
         /*
          * Cool, the item we want is already there
          */
         rc     = TDS_SUCCEED;
         result = REG_ROW;
      }
      else
      {
            /* 
             * XXX Note- we need to handle "compute" results as well.
             * I don't believe the current src/tds/token.c handles those
             * so we don't handle them yet either.
             */

            /* 
             * Get the row from the TDS stream.
             */
            rc = tds_process_row_tokens(dbproc->tds_socket);
            if (rc == TDS_SUCCEED)
            {
               /*
                * Add the row to the row buffer
                */
               buffer_add_row(&(dbproc->row_buf), resinfo->current_row, 
                              resinfo->row_size);
               result = REG_ROW;
            }
            else if (rc == TDS_NO_MORE_ROWS)
            {
               result = NO_MORE_ROWS;
            }
            else 
            {
               result = FAIL;
            }
      }
   
      if (result == REG_ROW)
      {
         /*
          * The data is in the row buffer, now transfer it to the 
          * bound variables
          */
         buffer_transfer_bound_data(&(dbproc->row_buf), dbproc, 
                                    dbproc->row_buf.next_row);
         dbproc->row_buf.next_row++;
      }
   }
   tdsdump_log(TDS_DBG_FUNC, "%L leaving dbnextrow() returning %d\n",result);
   return result;
} /* dbnextrow()  */

static int _db_get_server_type(int bindtype)
{
	switch (bindtype) {
		case CHARBIND:
		case STRINGBIND:
		case NTBSTRINGBIND:
			return SYBCHAR;
			break;
		case FLT8BIND:
			return SYBFLT8;
			break;
		case REALBIND:
			return SYBREAL;
			break;
		case INTBIND:
			return SYBINT4;
			break;
		case SMALLBIND:
			return SYBINT2;
			break;
		case TINYBIND:
			return SYBINT1;
			break;
          case DATETIMEBIND:
               return SYBDATETIME;
               break;
          case SMALLDATETIMEBIND:
               return SYBDATETIME4;
               break;
          case MONEYBIND:
               return SYBMONEY;
               break;
          case SMALLMONEYBIND:
               return SYBMONEY4;
               break;
          case BINARYBIND:
               return SYBBINARY;
               break;
          case VARYCHARBIND:
               return SYBVARCHAR;
               break;
          case BITBIND:
               return SYBBIT;
               break;
          case NUMERICBIND:
               return SYBNUMERIC;
               break;
          case DECIMALBIND:
               return SYBDECIMAL;
               break;
          default:
               return -1;
               break;
	}
}
/**
 * Conversion functions are handled in the TDS layer.
 * The main reason for this is that ctlib and ODBC (and presumably DBI) need
 * to be able to do conversions between datatypes. This is possible because
 * the format of complex data (dates, money, numeric, decimal) is defined by
 * its representation on the wire, thus what we call DBMONEY is exactly its
 * format on the wire. CLIs that need a differtent representation (ODBC?) 
 * need to convert from this format anyway, so the code would already be in
 * place.
 * The datatypes are also defined by its Server-type so all CLIs should be 
 * able to map native types to server types as well.
 *
 * tds_convert copies from src to dest and returns the output data length,
 * period.  All padding and termination is the responsibilty of the API library
 * and is done post conversion.  The peculiar rule in dbconvert() is that
 * a dest len of -1 and a desttype of SYBCHAR means the output buffer
 * should be null-teminated.  
 */
DBINT dbconvert(DBPROCESS *dbproc,
		int srctype,
		BYTE *src,
		DBINT srclen,
		int desttype,
		BYTE *dest,
		DBINT destlen)
{
TDS_UINT len;
DBINT length;

	if (srclen==-1)
		srclen = strlen((char *)src);
		
	/*
	 * a -1 destlen is legal, but we have to be more specific with
	 * tds_convert, and implies the output buffer should be null-teminated.
	 */
	len = (destlen == -1)? srclen : destlen;
	length = tds_convert (g_tds_context->locale, 
						srctype,  (TDS_CHAR *)src,  srclen, 
						desttype, (TDS_CHAR *)dest, len);
						
	if (length > 0 && destlen == -1 && desttype == SYBCHAR) 
		((TDS_CHAR *)dest)[length++] = '\0';
		
	return length;
}

RETCODE dbbind(
   DBPROCESS *dbproc,
   int        column,
   int        vartype,
   DBINT      varlen,
   BYTE      *varaddr)
{
   TDSCOLINFO    *colinfo = NULL;
   TDSRESULTINFO *resinfo = NULL;
   TDSSOCKET     *tds     = NULL;
   int            srctype = -1;   
   int            okay    = TRUE; /* so far, so good */

	dbproc->avail_flag = FALSE;
   /* 
    * Note on logic-  I'm using a boolean variable 'okay' to tell me if
    * everything that has happened so far has gone okay.  Basically if 
    * something happened that wasn't okay we don't want to keep doing 
    * things, but I also don't want to have a half dozen exit points from 
    * this function.  So basically I've wrapped each set of operation in a 
    * "if (okay)" statement.  Once okay becomes false we skip everything 
    * else.
    */
   okay = (dbproc!=NULL && dbproc->tds_socket!=NULL && varaddr!=NULL);

   if (okay)
   {
      tds     = (TDSSOCKET *) dbproc->tds_socket;
      resinfo = tds->res_info;
   }
   
   okay = okay && ((column >= 1) && (column <= resinfo->num_cols));

   if (okay)
   {
      colinfo  = resinfo->columns[column-1];
      srctype = tds_get_conversion_type(colinfo->column_type,
                                     colinfo->column_size);
      okay = okay && dbwillconvert(srctype, vartype);
   }

   if (okay)
   {   
      colinfo->varaddr         = (char *)varaddr;
      colinfo->column_bindtype = vartype;
      colinfo->column_bindlen  = varlen;
   }

   return okay ? SUCCEED : FAIL;
} /* dbbind()  */

void dbsetifile(char *filename)
{
	set_interfaces_file_loc(filename);
}
RETCODE dbnullbind(DBPROCESS *dbproc, int column, DBINT *indicator)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;

        /*
         *  XXX Need to check for possibly problems before assuming
         *  everything is okay
         */
	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[column-1];
	colinfo->column_nullbind = (TDS_CHAR *) indicator;

        return SUCCEED;
}
DBINT DBCOUNT(DBPROCESS *dbproc)
{
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo) 
		return resinfo->row_count;
	else return tds->rows_affected;
}
void dbclrbuf(DBPROCESS *dbproc, DBINT n)
{
   if (n <= 0) return;

   if (dbproc->row_buf.buffering_on)
   {
      if (n >= dbproc->row_buf.rows_in_buf) {
      	buffer_delete_rows(&(dbproc->row_buf), dbproc->row_buf.rows_in_buf - 1);
      } else {
      	buffer_delete_rows(&(dbproc->row_buf), n);
      }
   }
}
DBBOOL dbwillconvert(int srctype, int desttype)
{
   /* XXX */
   return TRUE;
}
int dbcoltype(DBPROCESS *dbproc,int column)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[column-1];
	switch (colinfo->column_type) {
		case SYBVARCHAR:
			return SYBCHAR; 
		case SYBVARBINARY:
			return SYBBINARY;
		case SYBDATETIMN:
			if (colinfo->column_size==8)
				return SYBDATETIME;
			else if (colinfo->column_size==4)
				return SYBDATETIME4;
		case SYBMONEYN:
			/* needs to be based on column size (mlilback, 11/7/01) */
			if (colinfo->column_size==4)
				return SYBMONEY4;
			else
				return SYBMONEY;
		case SYBFLTN:
			if (colinfo->column_size==8)
				return SYBFLT8;
			else if (colinfo->column_size==4)
				return SYBREAL;
		case SYBINTN:
			if (colinfo->column_size==4)
				return SYBINT4;
			else if (colinfo->column_size==2)
				return SYBINT2; 
			else if (colinfo->column_size==1)
				return SYBINT1; 
		default:
			return colinfo->column_type;
	}
	return 0; /* something went wrong */
}
DBTYPEINFO *dbcoltypeinfo(DBPROCESS *dbproc, int column)
{
/* moved typeinfo from static into dbproc structure to make thread safe. 
	(mlilback 11/7/01) */
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[column-1];
	dbproc->typeinfo.precision = colinfo->column_prec;
	dbproc->typeinfo.scale = colinfo->column_scale;
	return &dbproc->typeinfo;
}
char *dbcolsource(DBPROCESS *dbproc,int colnum)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[colnum-1];
	return colinfo->column_name;
}
DBINT dbcollen(DBPROCESS *dbproc, int column)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column<1 || column>resinfo->num_cols) return -1;
	colinfo = resinfo->columns[column-1];
	return colinfo->column_size;
}
/* dbvarylen(), pkleef@openlinksw.com 01/21/02 */
DBINT dbvarylen(DBPROCESS *dbproc, int column)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column<1 || column>resinfo->num_cols) 
		return FALSE;
	colinfo = resinfo->columns[column-1];

	if (tds_get_null (resinfo->current_row, column))
		return TRUE;

	switch (colinfo->column_type) {
		/* variable length fields */
		case SYBNVARCHAR:
		case SYBVARBINARY:
		case SYBVARCHAR:
				return TRUE;
		
		/* types that can be null */
		case SYBBITN:
		case SYBDATETIMN:
		case SYBDECIMAL:
		case SYBFLTN:
		case SYBINTN:
		case SYBMONEYN:
		case SYBNUMERIC:
				return TRUE;

		/* blob types */
		case SYBIMAGE:
		case SYBNTEXT:
		case SYBTEXT:
				return TRUE;
	}
	return FALSE;
}
DBINT dbdatlen(DBPROCESS *dbproc, int column)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;
DBINT ret;

	/* FIX ME -- this is the columns info, need per row info */
	/* Fixed by adding cur_row_size to colinfo, filled in by process_row
		in token.c. (mlilback, 11/7/01) */
	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column<1 || column>resinfo->num_cols) return -1;
	colinfo = resinfo->columns[column-1];
	tdsdump_log(TDS_DBG_INFO1, "%L dbdatlen() type = %d\n",colinfo->column_type);
	
	if (tds_get_null(resinfo->current_row,column-1))
		ret = 0;
	else
		ret = colinfo->cur_row_size;
	tdsdump_log(TDS_DBG_FUNC, "%L leaving dbdatlen() returning %d\n",ret);
	return ret;
}

BYTE *
dbdata(DBPROCESS *dbproc, int column)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;
TDS_VARBINARY *varbin;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column<1 || column>resinfo->num_cols) return NULL;

	colinfo = resinfo->columns[column-1];
	if (tds_get_null(resinfo->current_row,column-1)) {
		return NULL;
	}
	if (is_blob_type(colinfo->column_type)) {
		return (BYTE *)colinfo->column_textvalue;
	} 
	if (colinfo->column_type == SYBVARBINARY) {
		varbin = (TDS_VARBINARY *)
			&(resinfo->current_row[colinfo->column_offset]);
		return (BYTE *)varbin->array;
	}

	/* else */
	return &resinfo->current_row[colinfo->column_offset];
}

RETCODE dbcancel(DBPROCESS *dbproc)
{
   tds_send_cancel(dbproc->tds_socket);
   tds_process_cancel(dbproc->tds_socket);
   /* tds_process_default_tokens(dbproc->tds_socket,CANCEL); */
   return SUCCEED;
}
DBINT dbspr1rowlen(DBPROCESS *dbproc)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;
int i,col,len=0,collen,namlen;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;

	for (col=0;col<resinfo->num_cols;col++)
	{
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = strlen(colinfo->column_name);
		len += collen > namlen ? collen : namlen;
	}
	/* the space between each column */
	len += resinfo->num_cols - 1;
	/* the newline */
	len++;

	return len;
}
RETCODE dbspr1row(DBPROCESS *dbproc, char *buffer, DBINT buf_len)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;
int i,col,collen,namlen,len;
char dest[256];
int desttype, srctype;
int buf_cur=0;
RETCODE ret;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;

	buffer[0]='\0';

	if ((ret = dbnextrow(dbproc))!=REG_ROW) 
		return ret;

	for (col=0;col<resinfo->num_cols;col++)
	{
		colinfo = resinfo->columns[col];
		if (tds_get_null(resinfo->current_row,col)) {
			strcpy(dest,"NULL");
		} else {
			desttype = _db_get_server_type(STRINGBIND);
			srctype = tds_get_conversion_type(colinfo->column_type,colinfo->column_size);
			dbconvert(dbproc, srctype ,dbdata(dbproc,col+1), -1, desttype, (BYTE *)dest, 255);

		}
		collen = _get_printable_size(colinfo);
		namlen = strlen(colinfo->column_name);
		len = collen > namlen ? collen : namlen;
		for (i=strlen(dest);i<len;i++)
			strcat(dest," ");
		if (strlen(dest) + buf_cur < buf_len) {
			strcat(buffer,dest);
			buf_cur+=strlen(dest);
		}
		if (strlen(buffer)<buf_len) {
			strcat(buffer," ");
			buf_cur++;
		}
	}
	if (strlen(buffer)<buf_len) 
		strcat(buffer,"\n");
	return ret;
}

RETCODE
dbprrow(DBPROCESS *dbproc)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;
int i,col,collen,namlen,len;
char dest[256];
int desttype, srctype;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;

	while(dbnextrow(dbproc)==REG_ROW) {
		for (col=0;col<resinfo->num_cols;col++)
		{
			colinfo = resinfo->columns[col];
			if (tds_get_null(resinfo->current_row,col)) {
				strcpy(dest,"NULL");
			} else {
				desttype = _db_get_server_type(STRINGBIND);
				srctype = tds_get_conversion_type(colinfo->column_type,colinfo->column_size);
				dbconvert(dbproc, srctype ,dbdata(dbproc,col+1), -1, desttype, (BYTE *)dest, 255);

				/* printf ("some data\t"); */
			}
			printf("%s",dest);
			collen = _get_printable_size(colinfo);
			namlen = strlen(colinfo->column_name);
			len = collen > namlen ? collen : namlen;
			for (i=strlen(dest);i<len;i++)
				printf(" ");
			printf(" ");
		}
		printf ("\n");
	}
	return SUCCEED;
}
static int _get_printable_size(TDSCOLINFO *colinfo)
{
	switch (colinfo->column_type) {
		case SYBINTN:
			switch(colinfo->column_size) {
				case 1: return 3;
				case 2: return 6;
				case 4: return 11;
			}
		case SYBINT1:
			return 3;
		case SYBINT2:
			return 6;
		case SYBINT4:
        		return 11;
		case SYBVARCHAR:
		case SYBCHAR:
			return colinfo->column_size;
		case SYBFLT8:
			return 11; /* FIX ME -- we do not track precision */
		case SYBREAL:
			return 11; /* FIX ME -- we do not track precision */
		case SYBMONEY:
        		return 12; /* FIX ME */
		case SYBMONEY4:
        		return 12; /* FIX ME */
		case SYBDATETIME:
        		return 26; /* FIX ME */
		case SYBDATETIME4:
        		return 26; /* FIX ME */
		case SYBBIT:
		case SYBBITN:
			return 1;
		/* FIX ME -- not all types present */
		default:
			return 0;
	}

}
RETCODE dbsprline(DBPROCESS *dbproc,char *buffer, DBINT buf_len, DBCHAR line_char)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;
int i,col, len, collen, namlen;
char line_str[2],dest[256];
int buf_cur=0;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;

	buffer[0]='\0';
	line_str[0]=line_char;
	line_str[1]='\0';

	for (col=0;col<resinfo->num_cols;col++)
	{
		dest[0]='\0';
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = strlen(colinfo->column_name);
		len = collen > namlen ? collen : namlen;
		for (i=0;i<len;i++) 
			strcat(dest,line_str);
		if (strlen(dest)<buf_len-buf_cur) {
			strcat(buffer,dest);
			buf_cur += strlen(dest);
		}
		if (strlen(dest)<buf_len-buf_cur) {
			strcat(buffer," ");
			buf_cur++;
		}
	}
	if (strlen(dest)<buf_len-buf_cur) 
		strcat(buffer,"\n");
	return SUCCEED;
}
RETCODE dbsprhead(DBPROCESS *dbproc,char *buffer, DBINT buf_len)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;
int i,col, len, collen, namlen;
char dest[256];
int buf_cur=0;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;

	buffer[0]='\0';

	for (col=0;col<resinfo->num_cols;col++)
	{
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = strlen(colinfo->column_name);
		len = collen > namlen ? collen : namlen;
		strcpy(dest,colinfo->column_name);
		for (i=strlen(colinfo->column_name);i<len;i++)
			strcat(dest," ");
		if (strlen(dest) < buf_len-buf_cur) {
			strcat(buffer,dest);
			buf_cur += strlen(dest);
		}
		if (strlen(dest) < buf_len-buf_cur) {
			strcat(buffer," ");
			buf_cur++;
		}
	}
	if (strlen(dest) < buf_len-buf_cur) 
		strcat(buffer,"\n");
	return SUCCEED;
}
void dbprhead(DBPROCESS *dbproc)
{
TDSCOLINFO * colinfo;
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;
int i,col, len, collen, namlen;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	for (col=0;col<resinfo->num_cols;col++)
	{
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = strlen(colinfo->column_name);
		len = collen > namlen ? collen : namlen;
		printf("%s",colinfo->column_name);
		for (i=strlen(colinfo->column_name);i<len;i++)
			printf(" ");
		printf(" ");
	}
	printf ("\n");
	for (col=0;col<resinfo->num_cols;col++)
	{
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = strlen(colinfo->column_name);
		len = collen > namlen ? collen : namlen;
		for (i=0;i<len;i++)
			printf("-");
		printf(" ");
	}
	printf ("\n");

}

RETCODE
DBROWS(DBPROCESS *dbproc)
{
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo && resinfo->rows_exist) return SUCCEED;
	else return FAIL;
}
/* STUBS */
RETCODE dbsetdeflang(char *language)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetdeflang()\n");
	return SUCCEED;
}
int dbgetpacket(DBPROCESS *dbproc)
{
TDSSOCKET *tds = dbproc->tds_socket;

	if (!tds || !tds->env) {
		return TDS_DEF_BLKSZ;
	} else {
		return tds->env->block_size;
	}
}
RETCODE dbsetmaxprocs(int maxprocs)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetmaxprocs()\n");
	return SUCCEED;
}
RETCODE dbsettime(int seconds)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsettime()\n");
	return SUCCEED;
}
RETCODE dbsetlogintime(int seconds)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetlogintime()\n");
	return SUCCEED;
}
DBBOOL dbhasretstat(DBPROCESS *dbproc)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
	if (tds->has_status) {
		return TRUE;
	} else {
		return FALSE;
	}
}
DBINT dbretstatus(DBPROCESS *dbproc)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
	return tds->ret_status;
}
RETCODE DBCMDROW(DBPROCESS *dbproc)
{
        TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
        if (tds->res_info)
                return SUCCEED;
        return TDS_FAIL;
}
int dbaltcolid(DBPROCESS *dbproc, int computeid, int column)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbaltcolid()\n");
	return -1;
}
DBINT 
dbadlen(DBPROCESS *dbproc,int computeid, int column)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbaddlen()\n");
	return 0;
}
int dbalttype(DBPROCESS *dbproc, int computeid, int column)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbalttype()\n");
	return 0;
}
BYTE *dbadata(DBPROCESS *dbproc, int computeid, int column)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbadata()\n");
	return "";
}
int dbaltop(DBPROCESS *dbproc, int computeid, int column)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbaltop()\n");
	return -1;
}
RETCODE dbsetopt(DBPROCESS *dbproc, int option, char *char_param, int int_param)
{
   switch (option) {
      case DBBUFFER:
      {
         /* XXX should be more robust than just a atoi() */
         buffer_set_buffering(&(dbproc->row_buf), atoi(char_param));
         break;
      }
      default:
      {
         break;
      }
   }
   return SUCCEED;
}

void
dbsetinterrupt(DBPROCESS *dbproc, int (*ckintr)(),int (*hndlintr)())
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetinterrupt()\n");
}
int dbnumrets(DBPROCESS *dbproc)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbnumrets()\n");
	return 0;
}
char *dbretname(DBPROCESS *dbproc, int retnum)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbretname()\n");
	return NULL;
}
BYTE *dbretdata(DBPROCESS *dbproc, int retnum)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbretdata()\n");
	return NULL;
}
int dbretlen(DBPROCESS *dbproc, int retnum)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbretlen()\n");
	return -1;
}
RETCODE dbsqlok(DBPROCESS *dbproc)
{
unsigned char   marker;
RETCODE rc = SUCCEED;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;

	if (dbproc->text_sent) {
		tds_flush_packet(tds);
		dbproc->text_sent = 0;

		do {
			marker = tds_get_byte(tds);
			tds_process_default_tokens(tds, marker);
		} while (marker!=TDS_DONE_TOKEN);

		return SUCCEED;
	}
   /*
    * See what the next packet from the server is.  If it is an error
    * then we should return FAIL
    */
/* Calling dbsqlexec on an update only stored proc should read all tokens
	while (is_msg_token(tds_peek(tds))) {
		marker = tds_get_byte(tds);
		if (tds_process_default_tokens(tds, marker)!=TDS_SUCCEED) {
			rc=FAIL;
		}
	} 
*/
	do {
		marker = tds_peek(tds);
		if (!is_result_token(marker)) {
			marker = tds_get_byte(tds);
			/* tds_process_default_tokens can return TDS_ERROR in which case
			** we still want to read til end, but TDS_FAIL is an unrecoverable
			** error */
			if (tds_process_default_tokens(tds, marker)==TDS_FAIL) {
				rc=FAIL;
			}
		}
	} while (!is_hard_end_token(marker) && !is_result_token(marker));

	/* clean up */
	if (rc==TDS_ERROR && !is_end_token(marker)) {
		do {
			marker = tds_get_byte(tds);
			if (tds_process_default_tokens(tds, marker)!=TDS_SUCCEED) {
				return FAIL;
			}
		} while (marker!=TDS_DONE_TOKEN);
	}
	return rc;
}
int dbnumalts(DBPROCESS *dbproc,int computeid)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbnumalts()\n");
	return 0;
}
BYTE *
dbbylist(DBPROCESS *dbproc, int computeid, int *size)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbbylist()\n");
	if (size) *size = 0;
	return NULL;
}
DBBOOL DBDEAD(DBPROCESS *dbproc)
{
	if(dbproc &&
	dbproc->tds_socket &&
	dbproc->tds_socket->s)
		return FALSE;
	else
		return TRUE;
}

EHANDLEFUNC
dberrhandler(EHANDLEFUNC inHandler)
{
	EHANDLEFUNC old = g_dblib_err_handler;
	g_dblib_err_handler = inHandler;
	return old;
} 

EHANDLEFUNC
dberrhandle(EHANDLEFUNC handler)
{
   EHANDLEFUNC retFun = g_dblib_err_handler;

   g_dblib_err_handler = handler;
   return retFun;
}

MHANDLEFUNC
dbmsghandler(MHANDLEFUNC inHandler)
{
	MHANDLEFUNC old = g_dblib_msg_handler;
	g_dblib_msg_handler = inHandler;
	return old;
}

MHANDLEFUNC
dbmsghandle(MHANDLEFUNC handler)
{
   MHANDLEFUNC retFun = g_dblib_msg_handler;

   g_dblib_msg_handler = handler;
   return retFun;
}

RETCODE dbmnyadd(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *sum)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyadd()\n");
	return SUCCEED;
}
RETCODE dbmnysub(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *diff)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnysyb()\n");
	return SUCCEED;
}
RETCODE dbmnymul(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *prod)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnymul()\n");
	return SUCCEED;
}
RETCODE dbmnydivide(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2, DBMONEY *quotient)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnydivide()\n");
	return SUCCEED;
}
RETCODE dbmnycmp(DBPROCESS *dbproc, DBMONEY *m1, DBMONEY *m2)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnycmp()\n");
	return SUCCEED;
}
RETCODE dbmnyscale(DBPROCESS *dbproc, DBMONEY *dest, int multiplier, int addend)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyscale()\n");
	return SUCCEED;
}
RETCODE dbmnyzero(DBPROCESS *dbproc, DBMONEY *dest)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyzero()\n");
	return SUCCEED;
}
RETCODE dbmnymaxpos(DBPROCESS *dbproc, DBMONEY *dest)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnymaxpos()\n");
	return SUCCEED;
}
RETCODE dbmnymaxneg(DBPROCESS *dbproc, DBMONEY *dest)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnymaxneg()\n");
	return SUCCEED;
}
RETCODE dbmnyndigit(DBPROCESS *dbproc, DBMONEY *mnyptr,DBCHAR *value, DBBOOL *zero)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyndigit()\n");
	return SUCCEED;
}
RETCODE dbmnyinit(DBPROCESS *dbproc,DBMONEY *mnyptr, int trim, DBBOOL *negative)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyinit()\n");
	return SUCCEED;
}
RETCODE dbmnydown(DBPROCESS *dbproc,DBMONEY *mnyptr, int divisor, int *remainder)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnydown()\n");
	return SUCCEED;
}
RETCODE dbmnyinc(DBPROCESS *dbproc,DBMONEY *mnyptr)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyinc()\n");
	return SUCCEED;
}
RETCODE dbmnydec(DBPROCESS *dbproc,DBMONEY *mnyptr)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnydec()\n");
	return SUCCEED;
}
RETCODE dbmnyminus(DBPROCESS *dbproc,DBMONEY *src, DBMONEY *dest)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyminus()\n");
	return SUCCEED;
}
RETCODE dbmny4minus(DBPROCESS *dbproc, DBMONEY4 *src, DBMONEY4 *dest)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmny4minus()\n");
	return SUCCEED;
}
RETCODE dbmny4zero(DBPROCESS *dbproc, DBMONEY4 *dest)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmny4zero()\n");
	return SUCCEED;
}
RETCODE dbmny4add(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *sum)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmny4add()\n");
	return SUCCEED;
}
RETCODE dbmny4sub(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *diff)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmny4sub()\n");
	return SUCCEED;
}
RETCODE dbmny4mul(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *prod)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmny4mul()\n");
	return SUCCEED;
}
RETCODE dbmny4divide(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2, DBMONEY4 *quotient)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmny4divide()\n");
	return SUCCEED;
}
RETCODE dbmny4cmp(DBPROCESS *dbproc, DBMONEY4 *m1, DBMONEY4 *m2)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmny4cmp()\n");
	return SUCCEED;
}
RETCODE dbdatecmp(DBPROCESS *dbproc, DBDATETIME *d1, DBDATETIME *d2)
{
	if (d1->dtdays == d2->dtdays ) {
		if ( d1->dttime == d2->dttime )
			return 0;
		else
			return d1->dttime > d2->dttime ? 1 : -1 ;
	}

	/* date 1 is before 1900 */
	if (d1->dtdays > 2958463) {

		if (d2->dtdays > 2958463) /* date 2 is before 1900 */
			return d1->dtdays > d2->dtdays ? 1 : -1 ;
		else
			return -1;
	} else {
		/* date 1 is after 1900 */
		if (d2->dtdays < 2958463) /* date 2 is after 1900 */
			return d1->dtdays > d2->dtdays ? 1 : -1 ;
		else
			return 1;
	}
	return SUCCEED;
}
RETCODE dbdatecrack(DBPROCESS *dbproc, DBDATEREC *di, DBDATETIME *dt)
{
unsigned int dt_days;
unsigned int dt_time;


int dim[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
char mn[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

int dty, years, months, days, ydays, wday, hours, mins, secs, ms;
int i;

	dt_days = dt->dtdays;
	dt_time = dt->dttime;

	if (dt_days > 2958463) /* its a date before 1900 */ {
	  	dt_days = 0xffffffff - dt_days; 
/*
	 	dt_days = (unsigned int)4294967295U - dt_days;
*/
		wday = 7 - ( dt_days % 7); 
		years = -1;
		dty = days_this_year(years);

        
		while ( dt_days >= dty ) {
			years--; 
			dt_days -= dty;
			dty = days_this_year(years);
		}
		if (dty == 366 )
			dim[1] = 29;
		else
			dim[1] = 28;

		ydays = dty - dt_days;
		months = 11;
 
		while (dt_days > dim[months] ) {
			dt_days -= dim[months];
			months--;
		}

		days = dim[months] - dt_days;
	} else {
		wday = ( dt_days + 1 ) % 7; /* 'cos Jan 1 1900 was Monday */

		dt_days++;
		years = 0;
		dty = days_this_year(years);
		while ( dt_days > dty ) {
			years++; 
			dt_days -= dty;
			dty = days_this_year(years);
		}

		if (dty == 366 )
			dim[1] = 29;
		else
			dim[1] = 28;

		ydays = dt_days;
		months = 0;
		while (dt_days > dim[months] ) {
			dt_days -= dim[months];
			months++;
		}

		days = dt_days;
	}

	secs = dt_time / 300;
	ms = ((dt_time - (secs * 300)) * 1000) / 300 ;

	hours = 0;
	while ( secs >= 3600 ) {
		hours++; 
		secs -= 3600;
	}

	mins = 0;

	while ( secs >= 60 ) {
		mins++; 
		secs -= 60;
	}

#ifndef MSDBLIB
	di->dateyear    = 1900 + years;
	di->datemonth   = months;
	di->datedmonth  = days;
	di->datedyear   = ydays;
	di->datedweek   = wday;
	di->datehour    = hours;
	di->dateminute  = mins;
	di->datesecond  = secs;
	di->datemsecond = ms;
#else
	di->year        = 1900 + years;
	di->month       = months;
	di->day         = days;
	di->dayofyear   = ydays;
	di->weekday     = wday;
	di->hour        = hours;
	di->minute      = mins;
	di->second      = secs;
	di->millisecond = ms;
#endif
}

void dbrpwclr(LOGINREC *login)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbrpwclr()\n");
}
RETCODE dbrpwset(LOGINREC *login, char *srvname, char *password, int pwlen)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbrpwset()\n");
	return SUCCEED;
}
int dbspid(DBPROCESS *dbproc)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbspid()\n");
	return 0;
}
void
dbsetuserdata(DBPROCESS *dbproc, BYTE *ptr)
{
	dbproc->user_data = ptr;
	return;
}
BYTE *dbgetuserdata(DBPROCESS *dbproc)
{
	return dbproc->user_data;
}
RETCODE dbsetversion(DBINT version)
{
	g_dblib_version  = version;
	return SUCCEED;
}
RETCODE dbmnycopy(DBPROCESS *dbproc, DBMONEY *src, DBMONEY *dest)
{
	tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnycopy()\n");
	return SUCCEED;
}
RETCODE dbcanquery(DBPROCESS *dbproc)
{
	TDSSOCKET *tds;
	int rc;

	if (dbproc == NULL)
		return FAIL;
	tds = (TDSSOCKET *) dbproc->tds_socket;
	if (!tds || !tds->s) 
		return FAIL;

	/*
	 *  Just throw away all pending rows from the last query
	 */
	do {
		rc = tds_process_row_tokens(dbproc->tds_socket);
	} while (rc == TDS_SUCCEED);

	if (rc == TDS_FAIL)
		return FAIL;

	return SUCCEED;
}
void dbfreebuf(DBPROCESS *dbproc)
{
	if(dbproc->dbbuf) {
		free(dbproc->dbbuf);
		dbproc->dbbuf = NULL;
		}
		dbproc->dbbufsz = 0;
} /* dbfreebuf()  */

RETCODE dbclropt(DBPROCESS *dbproc,int option, char *param)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbcltopt()\n");
	return SUCCEED;
}
DBBOOL dbisopt(DBPROCESS *dbproc,int option, char *param)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbisopt()\n");
	return TRUE;
}
DBINT DBCURROW(DBPROCESS *dbproc)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED DBCURROW()\n");
	return 0;
}
int DBCURCMD(DBPROCESS *dbproc)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED DBCURCMD()\n");
	return 0;
}
RETCODE DBMORECMDS(DBPROCESS *dbproc)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED DBMORECMDS()\n");
	return SUCCEED;
}
int dbrettype(DBPROCESS *dbproc,int retnum)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbrettype()\n");
	return 0;
}
int dbstrlen(DBPROCESS *dbproc)
{
	return dbproc->dbbufsz;
}
char *dbgetchar(DBPROCESS *dbproc, int pos)
{
     if (dbproc->dbbufsz > 0) {
       if (pos >= 0 && pos < dbproc->dbbufsz )
            return (char*)&dbproc->dbbuf[pos];
       else
          return (char *)NULL;
     }
    else
        return (char *)NULL;
}
RETCODE dbstrcpy(DBPROCESS *dbproc, int start, int numbytes, char *dest)
{
	dest[0] = 0; /* start with empty string being returned */
	if (dbproc->dbbufsz>0) {
		strncpy(dest, (char*)&dbproc->dbbuf[start], numbytes);
	}
	return SUCCEED;
}
RETCODE dbsafestr(DBPROCESS *dbproc,char *src, DBINT srclen, char *dest, DBINT destlen, int quotetype)
{
	return SUCCEED;
}
char *dbprtype(int token)
{
   char  *result = NULL;


   switch (token)
   {
      case SYBINT1:         result = "tinyint";         break;
      case SYBINT2:         result = "smallint";        break;
      case SYBINT4:         result = "int";             break;
      case SYBMONEY:        result = "money";           break;
      case SYBFLT8:         result = "float";           break;
      case SYBDATETIME:     result = "datetime";        break;
      case SYBBIT:          result = "bit";             break;
      case SYBBITN:         result = "bit-null";        break;
      case SYBCHAR:         result = "char";            break;
      case SYBVARCHAR:      result = "varchar";         break;
      case SYBTEXT:         result = "text";            break;
      case SYBBINARY:       result = "binary";          break;
      case SYBVARBINARY:    result = "varbinary";       break;
      case SYBIMAGE:        result = "image";           break;
      case SYBDECIMAL:      result = "decimal";         break;
      case SYBNUMERIC:      result = "numeric";         break;
      case SYBINTN:         result = "integer-null";    break;
      case SYBDATETIMN:     result = "datetime-null";   break;
      case SYBMONEYN:       result = "money-null";      break;
      case SYBFLTN:         result = "float-null";      break;
      case SYBAOPSUM:       result = "sum";             break;
      case SYBAOPAVG:       result = "avg";             break;
      case SYBAOPCNT:       result = "count";           break;
      case SYBAOPMIN:       result = "min";             break;
      case SYBAOPMAX:       result = "max";             break;
      case SYBDATETIME4:    result = "smalldatetime";   break;
      case SYBMONEY4:       result = "smallmoney";      break;
      case SYBREAL:         result = "real";            break;
      default:              result = "";                break;
   }
   return result;
} /* dbprtype()  */
DBBINARY *dbtxtimestamp(DBPROCESS *dbproc, int column)
{
TDSSOCKET *tds;
TDSRESULTINFO * resinfo;

   tds = (TDSSOCKET *) dbproc->tds_socket;
   if (!tds->res_info) return NULL;
   resinfo = tds->res_info;
   if (column < 1 || column > resinfo->num_cols) return NULL;
   /* else */
   return (DBBINARY *) resinfo->columns[column-1]->column_timestamp;
}
DBBINARY *dbtxptr(DBPROCESS *dbproc,int column)
{
TDSSOCKET *tds;
TDSRESULTINFO * resinfo;

   tds = (TDSSOCKET *) dbproc->tds_socket;
   if (!tds->res_info) return NULL;
   resinfo = tds->res_info;
   if (column < 1 || column > resinfo->num_cols) return NULL;
   /* else */
   return (DBBINARY *) &resinfo->columns[column-1]->column_textptr;
}

RETCODE
dbwritetext(DBPROCESS *dbproc, char *objname, DBBINARY *textptr, DBTINYINT textptrlen, DBBINARY *timestamp, DBBOOL log, DBINT size, BYTE *text)
{
char query[1024];
char textptr_string[35]; /* 16 * 2 + 2 (0x) + 1 */
char timestamp_string[19]; /* 8 * 2 + 2 (0x) + 1 */
int marker;
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;

	tds_convert(g_tds_context->locale, SYBBINARY, (TDS_CHAR *)textptr, textptrlen, SYBCHAR, textptr_string, 35);
	tds_convert(g_tds_context->locale, SYBBINARY, (TDS_CHAR *)timestamp, 8, SYBCHAR, timestamp_string, 19);
	sprintf(query, "writetext bulk %s %s timestamp = %s",
		objname, textptr_string, timestamp_string); 
	if (tds_submit_query(dbproc->tds_socket, query)!=TDS_SUCCEED) {
		return FAIL;
	}
	
	/* read the end token */
	marker = tds_get_byte(dbproc->tds_socket);

   	if (!dbproc->tds_socket->s) return FAIL;

	tds_process_default_tokens(dbproc->tds_socket, marker);
	if (marker != TDS_DONE_TOKEN) {
		return FAIL;
	}
	
	dbproc->tds_socket->out_flag=0x07;
	tds_put_int(dbproc->tds_socket, size);

	if (!text) {
		dbproc->text_size = size;
		dbproc->text_sent = 0;
		return SUCCEED;
	}

	tds_put_bulk_data(dbproc->tds_socket, text, size);
	tds_flush_packet(dbproc->tds_socket);

	do {
		marker = tds_get_byte(dbproc->tds_socket);
		tds_process_default_tokens(dbproc->tds_socket, marker);
	} while (marker!=TDS_DONE_TOKEN);

	return SUCCEED;
}
STATUS dbreadtext(DBPROCESS *dbproc, void *buf, DBINT bufsize)
{
TDSSOCKET *tds;
TDSCOLINFO *curcol;
int cpbytes, bytes_avail, rc;

	tds = dbproc->tds_socket;

	if (!tds || !tds->res_info || !tds->res_info->columns[0])
		return -1;

	curcol = tds->res_info->columns[0];

	/* if the current position is beyond the end of the text
	** set pos to 0 and return 0 to denote the end of the 
	** text */
	if (curcol->column_textpos &&
	   curcol->column_textpos>=curcol->column_textsize) {
		curcol->column_textpos = 0;
		return 0;
	}

	/* if pos is 0 (first time through or last call exhausted the text)
	** then read another row */ 
	if (curcol->column_textpos==0) {
        	rc = tds_process_row_tokens(dbproc->tds_socket);
        	if (rc != TDS_SUCCEED) {
			return NO_MORE_ROWS;
		}
	}

	/* find the number of bytes to return */
	bytes_avail = curcol->column_textsize - curcol->column_textpos;
	cpbytes = bytes_avail > bufsize ? bufsize : bytes_avail;
	memcpy(buf,&curcol->column_textvalue[curcol->column_textpos],cpbytes);
	curcol->column_textpos += cpbytes;
	return cpbytes;
}
RETCODE dbmoretext(DBPROCESS *dbproc, DBINT size, BYTE *text)
{
int marker;

	tds_put_bulk_data(dbproc->tds_socket, text, size);
	dbproc->text_sent += size;

	return SUCCEED;
}
void dbrecftos(char *filename)
{
}
char *dbversion()
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbversion()\n");
	return NULL;
}
RETCODE dbsetdefcharset(char *charset)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetdefcharset()\n");
	return SUCCEED;
}
RETCODE dbreginit(
      DBPROCESS *dbproc,
      DBCHAR *procedure_name,
      DBSMALLINT namelen )
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbreginit()\n");
        return SUCCEED;
}
RETCODE dbreglist(DBPROCESS *dbproc)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbreglist()\n");
        return SUCCEED;
}
RETCODE dbregparam(
      DBPROCESS    *dbproc,
      char         *param_name,
      int          type,
      DBINT        datalen,
      BYTE         *data )
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbregparam()\n");
        return SUCCEED;
}
RETCODE dbregexec(
      DBPROCESS      *dbproc,
      DBUSMALLINT    options)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbregexec()\n");
	return SUCCEED;
}
char      *dbmonthname(DBPROCESS *dbproc,char *language,int monthnum,DBBOOL shortform)
{
char *shortmon[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
char *longmon[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};

	if (shortform)
		return shortmon[monthnum-1];
	else
		return longmon[monthnum-1];
}
char      *dbname(DBPROCESS *dbproc)
{
	return NULL;
}
RETCODE dbsqlsend(DBPROCESS *dbproc)
{
int   result = FAIL;
TDSSOCKET *tds;

	dbproc->avail_flag = FALSE;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	if (tds->res_info && tds->res_info->more_results) {
      /* 
       * XXX If I read the documentation correctly it gets a
       * bit more complicated than this.
       *
       * You see if the person did a query and retrieved all 
       * the rows but didn't call dbresults() and if the query 
       * didn't return multiple results then this routine should
       * just end the TDS_DONE_TOKEN packet and be done with it.
       *
       * Unfortunately the only way we can know that is by peeking 
       * ahead to the next byte.  Peeking could block and this is supposed
       * to be a non-blocking call.  
       *
       */
 
      result = FAIL;
   }
   else
   {
      dbproc->more_results = TRUE;
      dbproc->empty_res_hack = 0;
      if (tds_submit_query(dbproc->tds_socket, (char *)dbproc->dbbuf)!=TDS_SUCCEED) {
	return FAIL;
      }
      if (! dbproc->noautofree)
      {
			dbfreebuf(dbproc);
      }
      result = SUCCEED;
   }
   return result;
}
RETCODE dbaltutype(DBPROCESS *dbproc, int computeid, int column)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbaltutype()\n");
	return SUCCEED;
}
RETCODE dbaltlen(DBPROCESS *dbproc, int computeid, int column)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbaltlen()\n");
	return SUCCEED;
}
RETCODE dbpoll(DBPROCESS *dbproc, long milliseconds, DBPROCESS **ready_dbproc, int *return_reason)
{
        tdsdump_log (TDS_DBG_FUNC, "%L UNIMPLEMENTED dbpoll()\n");
	return SUCCEED;
}

DBINT DBLASTROW(DBPROCESS *dbproc)
{
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	return resinfo->row_count;
#if 0
  DBINT   result;

   if (dbproc->row_buf.rows_in_buf == 0)
   {
      result = 0;
   }
   else
   {
      /* rows returned from the row buffer start with 1 instead of 0
      ** newest is 0 based. */
      result = dbproc->row_buf.newest + 1;
   }
   return result;
#endif
}

DBINT DBFIRSTROW(DBPROCESS *dbproc)
{
   DBINT   result;

   if (dbproc->row_buf.rows_in_buf == 0)
   {
      result = 0;
   }
   else
   {
      result = dbproc->row_buf.oldest;
   }
   return result;
}
int DBIORDESC(DBPROCESS *dbproc)
{
   return dbproc->tds_socket->s;
}
int DBIOWDESC(DBPROCESS *dbproc)
{
   return dbproc->tds_socket->s;
}
 
static void _set_null_value(DBPROCESS *dbproc, BYTE *varaddr, int datatype, int maxlen)
{
	switch (datatype) {
		case SYBINT4:
			memset(varaddr,'\0',4);
			break;
		case SYBINT2:
			memset(varaddr,'\0',2);
			break;
		case SYBINT1:
			memset(varaddr,'\0',1);
			break;
		case SYBFLT8:
			memset(varaddr,'\0',8);
			break;
		case SYBREAL:
			memset(varaddr,'\0',4);
			break;
		case SYBCHAR:
		case SYBVARCHAR:
			varaddr[0]='\0';
			break;
	}
}
DBBOOL DBISAVAIL(DBPROCESS *dbproc)
{
	return dbproc->avail_flag;
}
void dbsetavail(DBPROCESS *dbproc)
{
	dbproc->avail_flag = TRUE;
}
