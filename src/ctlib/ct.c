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
#include <stdlib.h>
#include <unistd.h>
#include <ctpublic.h>
#include <ctlib.h>
#include "tdsutil.h"

static char  software_version[]   = "$Id: ct.c,v 1.15 2002-02-17 20:23:37 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

/* these function pointers are called by the tds layer to propagate message
** back up to the CLI */
extern int (*g_tds_msg_handler)();
extern int (*g_tds_err_handler)();

static void _ct_bind_data(CS_COMMAND *cmd);


CS_RETCODE ct_exit(CS_CONTEXT *ctx, CS_INT unused)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_exit()\n");
	return CS_SUCCEED;
}
CS_RETCODE ct_init(CS_CONTEXT *ctx, CS_INT version)
{
	/* uncomment the next line to get pre-login trace */
	/* tdsdump_open("/tmp/tds2.log"); */
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_init()\n");
   /* set the functions in the TDS layer to point to the correct info/err
    * message handler functions */
   g_tds_msg_handler = ctlib_handle_info_message;
   g_tds_err_handler = ctlib_handle_err_message;
	return CS_SUCCEED;
}
CS_RETCODE ct_con_alloc(CS_CONTEXT *ctx, CS_CONNECTION **con)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_con_alloc()\n");
	*con = (CS_CONNECTION *) malloc(sizeof(CS_CONNECTION));
	memset(*con,'\0',sizeof(CS_CONNECTION));
	(*con)->tds_login = (void *) tds_alloc_login();

	/* so we know who we belong to */
	(*con)->ctx = ctx;

	/* set default values */
	tds_set_library((*con)->tds_login, "CT-Library");
	/* tds_set_charset((*con)->tds_login, "iso_1"); */
	/* tds_set_packet((*con)->tds_login, TDS_DEF_BLKSZ); */
	return CS_SUCCEED;
}
CS_RETCODE ct_callback(CS_CONTEXT *ctx, CS_CONNECTION *con, CS_INT action, CS_INT type, CS_VOID *func)
{
	int	(*funcptr)(void*,void*,void*)=(int (*)(void*,void*,void*))func;

	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_callback() action = %s\n",
		CS_GET ? "CS_GET" : "CS_SET");
	/* one of these has to be defined */
	if (!ctx && !con) 
		return CS_FAIL;

	if (action==CS_GET) {
		switch(type) {
			case CS_CLIENTMSG_CB:
				*(void **)func = (CS_VOID *) (con ? con->_clientmsg_cb : ctx->_clientmsg_cb);
				return CS_SUCCEED;
			case CS_SERVERMSG_CB:
				*(void **)func = (CS_VOID *) (con ? con->_servermsg_cb : ctx->_servermsg_cb);
				return CS_SUCCEED;
			default:
				fprintf(stderr,"Unknown callback %d\n",type);
				*(void **)func = (CS_VOID *) NULL;
				return CS_SUCCEED;
		}
	}
	/* CS_SET */
	switch(type) {
		case CS_CLIENTMSG_CB:
			if (con) 
				con->_clientmsg_cb = funcptr;
			else 
				ctx->_clientmsg_cb = funcptr;
			break;
		case CS_SERVERMSG_CB:
			if (con) 
				con->_servermsg_cb = funcptr;
			else 
				ctx->_servermsg_cb = funcptr;
			break;
	}
	return CS_SUCCEED;
}

CS_RETCODE ct_con_props(CS_CONNECTION *con, CS_INT action, CS_INT property,
CS_VOID *buffer, CS_INT buflen, CS_INT *out_len)
{
CS_INT intval = 0, maxcp;
TDSSOCKET *tds;
TDSLOGIN *tds_login;
char *set_buffer = NULL;

	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_con_props() action = %s property = %d\n",
		CS_GET ? "CS_GET" : "CS_SET", property);

	tds = con->tds_socket;
	tds_login = con->tds_login;

	if (action==CS_SET) {
		if (property == CS_USERNAME || property == CS_PASSWORD
		 || property == CS_APPNAME  || property == CS_HOSTNAME) {
			if (buflen == CS_NULLTERM) {
				maxcp = strlen(buffer);
				set_buffer = (char *)malloc(maxcp + 1);
				strcpy(set_buffer, buffer);
			} else if (buflen == CS_UNUSED) {
				return CS_SUCCEED;
			} else {
				set_buffer = (char *)malloc(buflen + 1);
				strncpy(set_buffer, buffer, buflen);
				set_buffer[buflen] = '\0';
			}
		}

		/* XXX "login" properties shouldn't be set after
		 * login.  I don't know if it should fail silently
		 * or return an error.
		 */
		switch (property) {
			case CS_USERNAME:
				tds_set_user(tds_login, set_buffer);
				break;
			case CS_PASSWORD:
				tds_set_passwd(tds_login, set_buffer);
				break;
			case CS_APPNAME:
				tds_set_app(tds_login, set_buffer);
				break;
			case CS_HOSTNAME:
				tds_set_host(tds_login, set_buffer);
				break;
			case CS_LOC_PROP:
				con->locale = (CS_LOCALE *)buffer;
				break;
			case CS_USERDATA:
				if (con->userdata) {
					free(con->userdata);
				}
				con->userdata = (void *)malloc(buflen+1);
				tdsdump_log(TDS_DBG_INFO2, "%L setting userdata orig %d new %d\n",buffer,con->userdata);
				con->userdata_len = buflen;
				memcpy(con->userdata, buffer, buflen);
				break;
			case CS_BULK_LOGIN:
				memcpy(&intval, buffer, sizeof(intval));
				if (intval) 
					tds_set_bulk(tds_login,1);
				else 
					tds_set_bulk(tds_login,0);
				break;
			case CS_PACKETSIZE:
				memcpy(&intval, buffer, sizeof(intval));
				tds_set_packet(tds_login, (short)intval);
				break;
			case CS_TDS_VERSION:
			/* FIX ME
			 * (a) We don't support all versions in tds/login.c -
			 *     I tried to pick reasonable versions.
			 * (b) Might need support outside of tds/login.c
			 * (c) It's a "negotiated" property so probably
			 *     needs tds_process_env_chg() support
			 * (d) Minor - we don't check against context
			 *     which should limit the acceptable values
			 */
				if (*(int *)buffer == CS_TDS_40) {
					tds_set_version(tds_login, 4, 2);
				} else if (*(int *)buffer == CS_TDS_42) {
					tds_set_version(tds_login, 4, 2);
				} else if (*(int *)buffer == CS_TDS_46) {
					tds_set_version(tds_login, 4, 6);
				} else if (*(int *)buffer == CS_TDS_495) {
					tds_set_version(tds_login, 4, 6);
				} else if (*(int *)buffer == CS_TDS_50) {
					tds_set_version(tds_login, 5, 0);
				} else if (*(int *)buffer == CS_TDS_70) {
					tds_set_version(tds_login, 7, 0);
				} else {
					return CS_FAIL;
				}
				break;
			default:
				tdsdump_log(TDS_DBG_ERROR, "%L Unknown property %d\n",property);
				break;
		}
		if (set_buffer) free(set_buffer);
	} else if (action==CS_GET) {
		switch (property) {
			case CS_USERNAME:
				maxcp = strlen(tds_login->user_name);
				if (out_len) *out_len=maxcp;
				if (maxcp>=buflen) maxcp = buflen-1;
				strncpy(buffer,tds_login->user_name,maxcp);
				((char *)buffer)[maxcp]='\0';
				break;
			case CS_PASSWORD:
				maxcp = strlen(tds_login->password);
				if (out_len) *out_len=maxcp;
				if (maxcp>=buflen) maxcp = buflen-1;
				strncpy(buffer,tds_login->password,maxcp);
				((char *)buffer)[maxcp]='\0';
				break;
			case CS_APPNAME:
				maxcp = strlen(tds_login->app_name);
				if (out_len) *out_len=maxcp;
				if (maxcp>=buflen) maxcp = buflen-1;
				strncpy(buffer,tds_login->app_name,maxcp);
				((char *)buffer)[maxcp]='\0';
				break;
			case CS_HOSTNAME:
				maxcp = strlen(tds_login->host_name);
				if (out_len) *out_len=maxcp;
				if (maxcp>=buflen) maxcp = buflen-1;
				strncpy(buffer,tds_login->host_name,maxcp);
				((char *)buffer)[maxcp]='\0';
				break;
			case CS_LOC_PROP:
				buffer = (CS_VOID *)con->locale;
				break;
			case CS_USERDATA:
				tdsdump_log(TDS_DBG_INFO2, "%L fetching userdata %d\n",con->userdata);
				maxcp = con->userdata_len;
				if (out_len) *out_len=maxcp;
				if (maxcp>buflen) maxcp = buflen;
				memcpy(buffer, con->userdata, maxcp);
				break;
			case CS_CON_STATUS:
				if (tds && tds->s) 
					intval |= CS_CONSTAT_CONNECTED;
				else 
					intval &= ~CS_CONSTAT_CONNECTED;
				if (tds && tds->state==TDS_DEAD) 
					intval |= CS_CONSTAT_DEAD;
				else
					intval &= ~CS_CONSTAT_DEAD;
				memcpy(buffer, &intval, sizeof(intval));
				break;
			case CS_BULK_LOGIN:
				if (tds_login->bulk_copy)
					intval=CS_FALSE;
				else 
					intval=CS_TRUE;
				memcpy(buffer, &intval, sizeof(intval));
				break;
			case CS_PACKETSIZE:
				if (tds && tds->env)
					intval = tds->env->block_size;
				else
					intval = tds_login->block_size;
				memcpy(buffer, &intval, sizeof(intval));
				if (out_len) *out_len=sizeof(intval);
				break;
			default:
				tdsdump_log(TDS_DBG_ERROR, "%L Unknown property %d\n",property);
				break;
		}
	}
	return CS_SUCCEED;
}

CS_RETCODE ct_connect(CS_CONNECTION *con, CS_CHAR *servername, CS_INT snamelen)
{
char *server;
int needfree=0;
CS_CONTEXT *ctx;

	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_connect() servername = %s\n", servername);

	if (snamelen==0 || snamelen==CS_UNUSED) {
		server=NULL;
	} else if (snamelen==CS_NULLTERM) {
		server=(char *)servername;
	} else {
		server = (char *) malloc(snamelen+1);
		needfree++;
		strncpy(server,servername,snamelen);
		server[snamelen]='\0';
	}
        tds_set_server(con->tds_login,server);
	   ctx = con->ctx;
        if (!(con->tds_socket = (void *) tds_connect(con->tds_login, ctx->locale, (void *) con))) {
		if (needfree) free(server);
		tdsdump_log(TDS_DBG_FUNC, "%L leaving ct_connect() returning %d\n", CS_FAIL);
		return CS_FAIL;
	}

        /* tds_set_parent( con->tds_socket, con); */

	if (needfree) free(server);

	tdsdump_log(TDS_DBG_FUNC, "%L leaving ct_connect() returning %d\n", CS_SUCCEED);
	return CS_SUCCEED;
}
CS_RETCODE ct_cmd_alloc(CS_CONNECTION *con, CS_COMMAND **cmd)
{

	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_cmd_alloc()\n");

	*cmd = (CS_COMMAND *) malloc(sizeof(CS_COMMAND));
	memset(*cmd,'\0',sizeof(CS_COMMAND));

	/* so we know who we belong to */
	(*cmd)->con = con;

	return CS_SUCCEED;
}
CS_RETCODE ct_command(CS_COMMAND *cmd, CS_INT type, CS_VOID *buffer, CS_INT buflen, CS_INT option)
{
int query_len;

	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_command()\n");
	/* FIX ME -- will only work for type CS_LANG_CMD */
	if (buflen==CS_NULLTERM) {
		query_len = strlen(buffer);
	} else {
		query_len = buflen;
	}
	if (cmd->query) free(cmd->query);
	cmd->query = (char *) malloc(query_len + 1);
	strncpy(cmd->query,(char *)buffer,query_len);
	cmd->query[query_len]='\0';

	return CS_SUCCEED;
}
CS_RETCODE ct_send_dyn(CS_COMMAND *cmd)
{
	if (cmd->dynamic_cmd==CS_PREPARE) {
		cmd->dynamic_cmd=0;
		if (tds_submit_prepare(cmd->con->tds_socket, cmd->query, cmd->dyn_id)==TDS_FAIL)
			return CS_FAIL;
		else 
			return CS_SUCCEED;
	} else if (cmd->dynamic_cmd==CS_EXECUTE) {
		cmd->dynamic_cmd=0;
		if (tds_submit_execute(cmd->con->tds_socket, cmd->dyn_id)==TDS_FAIL)
			return CS_FAIL;
		else 
			return CS_SUCCEED;
	}
}
CS_RETCODE ct_send(CS_COMMAND *cmd)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_send()\n");
	if (cmd->dynamic_cmd) 
		return ct_send_dyn(cmd);

	if (tds_submit_query(cmd->con->tds_socket, cmd->query)==TDS_FAIL) {
		tdsdump_log(TDS_DBG_WARN, "%L ct_send() failed\n");
		return CS_FAIL;
	} else {
		tdsdump_log(TDS_DBG_INFO2, "%L ct_send() succeeded\n");
		return CS_SUCCEED;
	}
}
CS_RETCODE ct_results_dyn(CS_COMMAND *cmd, CS_INT *result_type)
{
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;
TDSDYNAMIC *dyn;

	tds = cmd->con->tds_socket;

	if (cmd->dynamic_cmd==CS_DESCRIBE_INPUT) {
		dyn = tds->dyns[tds->cur_dyn_elem];
		if (dyn->dyn_state) {
			dyn->dyn_state = 0;
			return CS_END_RESULTS;
		} else {
			dyn->dyn_state++;
			*result_type = CS_DESCRIBE_RESULT;
			return CS_SUCCEED;
		}
	}

}
CS_RETCODE ct_results(CS_COMMAND *cmd, CS_INT *result_type)
{
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;
int ret;

	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_results()\n");

	if (cmd->dynamic_cmd) {
		return ct_results_dyn(cmd, result_type);
	}

	if (cmd->cmd_done) {
		cmd->cmd_done = 0;
		*result_type = CS_CMD_DONE;
		return CS_SUCCEED;
	} 

	tds = cmd->con->tds_socket;

	switch (ret = tds_process_result_tokens(tds)) {
		case TDS_SUCCEED:
			resinfo = tds->res_info;
			if (resinfo->rows_exist) {
				*result_type = CS_ROW_RESULT;
				cmd->cmd_done = 1;
			} else {
				*result_type = CS_CMD_SUCCEED;
			}
			return CS_SUCCEED;
		case TDS_NO_MORE_RESULTS:
			if (! tds->res_info) {
				if (cmd->empty_res_hack) {
					cmd->empty_res_hack=0;
					*result_type = CS_CMD_DONE;
					return CS_END_RESULTS;
				} else {
					cmd->empty_res_hack=1;
					*result_type = CS_CMD_SUCCEED;
					return CS_SUCCEED;
				}
			}	
			if (tds->res_info && !tds->res_info->rows_exist) {
				if (cmd->empty_res_hack) {
					cmd->empty_res_hack=0;
					*result_type = CS_CMD_DONE;
					return CS_END_RESULTS;
				} else {
					cmd->empty_res_hack=1;
					*result_type = CS_ROW_RESULT;
					return CS_SUCCEED;
				}
			} else {
				*result_type = CS_CMD_DONE;
				return CS_END_RESULTS;
			}
		case TDS_FAIL:
			if (tds->state == TDS_DEAD) {
				return CS_FAIL;
			} else {
				*result_type = CS_CMD_FAIL;
				return CS_SUCCEED;
			}
		default:
			return CS_FAIL;
	}	
	return CS_FAIL;
}
CS_RETCODE ct_bind(CS_COMMAND *cmd, CS_INT item, CS_DATAFMT *datafmt, CS_VOID *buffer, CS_INT *copied, CS_SMALLINT *indicator)
{
TDSCOLINFO * colinfo;
CT_COLINFO *ctcolinfo; /* additional information about columns used by ctlib */
TDSRESULTINFO * resinfo;
TDSSOCKET * tds;

   tdsdump_log(TDS_DBG_FUNC, "%L inside ct_bind()\n");

   tds = (TDSSOCKET *) cmd->con->tds_socket;
   resinfo = tds->res_info;
   colinfo = resinfo->columns[item-1];
   colinfo->varaddr = (char *)buffer;
   colinfo->column_bindtype = datafmt->datatype;
   colinfo->column_bindfmt = datafmt->format;
   tdsdump_log(TDS_DBG_INFO1, "%L inside ct_bind() item = %d datafmt->datatype = %d\n", item, datafmt->datatype);
   colinfo->column_bindlen = datafmt->maxlength;
   if (indicator) {
   	colinfo->column_nullbind = (TDS_CHAR *) indicator;
   }
   if (copied) {
   	colinfo->column_lenbind = (TDS_CHAR *) copied;
   }
   return CS_SUCCEED;
}

CS_RETCODE ct_fetch(CS_COMMAND *cmd, CS_INT type, CS_INT offset, CS_INT option, CS_INT *rows_read)
{
   tdsdump_log(TDS_DBG_FUNC, "%L inside ct_fetch()\n");

   switch (tds_process_row_tokens(cmd->con->tds_socket)) {
      case TDS_SUCCEED:
      	if (rows_read) *rows_read = 1;
      	_ct_bind_data(cmd);
      	return TDS_SUCCEED;
      case TDS_NO_MORE_ROWS:
      	if (rows_read) *rows_read = 0;
      	return CS_END_DATA;
      default:
      	if (rows_read) *rows_read = 0;
      	return CS_FAIL;
   }
   return CS_SUCCEED;
}

static void _ct_bind_data(CS_COMMAND *cmd)
{
int i;
TDSCOLINFO *curcol;
TDSSOCKET *tds = cmd->con->tds_socket;
TDSRESULTINFO *resinfo = tds->res_info;
unsigned char *src;
unsigned char *dest;
TDS_INT srctype, srclen, desttype, destlen, len;
CS_CONTEXT *ctx = cmd->con->ctx;

   tdsdump_log(TDS_DBG_FUNC, "%L inside _ct_bind_data()\n");

   for (i=0; i<resinfo->num_cols; i++) {
      curcol = resinfo->columns[i];

      if (curcol->column_nullbind) {
      	if (tds_get_null(resinfo->current_row,i)) {
         		*((CS_SMALLINT *)curcol->column_nullbind) = -1;
      	} else {
         		*((CS_SMALLINT *)curcol->column_nullbind) = 0;
      	}
	 }
      /* printf("%d %s\n",i,resinfo->columns[i]->column_value); */
   
      srctype = curcol->column_type;
      desttype = _ct_get_server_type(curcol->column_bindtype);
      dest = (unsigned char *)curcol->varaddr;
      tdsdump_log(TDS_DBG_INFO1, "%L inside _ct_bind_data() converting column %d from type %d to type %d\n", 
         i, srctype, desttype);
   
      if (dest && !tds_get_null(resinfo->current_row,i)) {
         srctype = tds_get_conversion_type(srctype, curcol->column_size);
         destlen = curcol->column_bindlen;
         if (is_blob_type(srctype)) {
            src = (unsigned char *)curcol->column_textvalue;
            srclen = curcol->column_textsize;
         } else {
            src = &(resinfo->current_row[curcol->column_offset]);
            srclen = curcol->column_size;
         }
         tdsdump_log(TDS_DBG_INFO1, "%L inside _ct_bind_data() setting source length for %d = %d\n", i, srclen);
         len = tds_convert(ctx->locale, srctype, (TDS_CHAR *)src, srclen, desttype, (TDS_CHAR *)dest, destlen);
         tdsdump_log(TDS_DBG_INFO2, "%L inside _ct_bind_data() conversion done len = %d bindfmt = %d\n", len, curcol->column_bindfmt);
   
         /* XXX need utf16 support (NCHAR,NVARCHAR,NTEXT) */
         switch (curcol->column_bindfmt) {
            case CS_FMT_NULLTERM:
               if (desttype==SYBCHAR || desttype==SYBVARCHAR
                || desttype==SYBTEXT) {
                  if (destlen > len)  dest[len] = '\0';
                  else                dest[len-1] = '\0';
                  len++; /* CS_FMT_NULLTERM includes the null in the size */
               }
               break;      
            case CS_FMT_UNUSED:
               break;      
            case CS_FMT_PADBLANK:
               if (desttype==SYBCHAR || desttype==SYBVARCHAR
                || desttype==SYBTEXT) {
                  if (destlen > len) {
                      memset(dest+len, (int)' ', destlen - len);
                  }
               }
               break;      
            case CS_FMT_PADNULL:
               if (desttype==SYBCHAR   || desttype==SYBVARCHAR
                || desttype==SYBBINARY || desttype==SYBVARBINARY
                || desttype==SYBTEXT   || desttype==SYBIMAGE) {
                  if (destlen > len) {
                      memset(dest+len, 0, destlen - len);
                  }
               }
               break;      
            default:
               break;
         }
         if (curcol->column_lenbind) {
            tdsdump_log(TDS_DBG_INFO1, "%L inside _ct_bind_data() length binding len = %d\n", len);
            *((CS_INT *)curcol->column_lenbind) = len;
         }
      }
   }
}

CS_RETCODE ct_cmd_drop(CS_COMMAND *cmd)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_cmd_drop()\n");
	if (cmd) {
		if (cmd->query) free(cmd->query);
		free(cmd);
	}
	return CS_SUCCEED;
}
CS_RETCODE ct_close(CS_CONNECTION *con, CS_INT option)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_close()\n");
	tds_free_socket(con->tds_socket);
	return CS_SUCCEED;
}


CS_RETCODE ct_con_drop(CS_CONNECTION *con)
{
   tdsdump_log(TDS_DBG_FUNC, "%L inside ct_con_drop()\n");
   if (con) {
      if (con->userdata)   free(con->userdata);
      if (con->tds_login)  tds_free_login(con->tds_login);
      free(con);
   }
   return CS_SUCCEED;
}


int _ct_get_client_type(int datatype, int size)
{
   tdsdump_log(TDS_DBG_FUNC, "%L inside _ct_get_client_type()\n");
   switch (datatype) {
         case SYBBIT:
         case SYBBITN:
		return CS_BIT_TYPE;
		break;
         case SYBCHAR:
         case SYBVARCHAR:
		return CS_CHAR_TYPE;
		break;
         case SYBINT4:
      		return CS_INT_TYPE;
		break;
         case SYBINT2:
      		return CS_SMALLINT_TYPE;
		break;
         case SYBINT1:
      		return CS_TINYINT_TYPE;
		break;
	 case SYBINTN:
		if (size==4) {
			return CS_INT_TYPE;
		} else if (size==2) {
      			return CS_SMALLINT_TYPE;
		} else if (size==1) {
      			return CS_TINYINT_TYPE;
		} else {
			fprintf(stderr,"Unknown size %d for SYBINTN\n", size);
		}
		break;
         case SYBREAL:
      		return CS_REAL_TYPE;
		break;
         case SYBFLT8:
      		return CS_FLOAT_TYPE;
		break;
	 case SYBFLTN:
      		if (size==4) {
			return CS_REAL_TYPE;
		} else if (size==8) {
			return CS_FLOAT_TYPE;
		} else {
			fprintf(stderr,"Error! unknown float size of %d\n",size);
		}
         case SYBMONEY:
      		return CS_MONEY_TYPE;
		break;
         case SYBMONEY4:
      		return CS_MONEY4_TYPE;
		break;
	 case SYBMONEYN:
      		if (size==4) {
			return CS_MONEY4_TYPE;
		} else if (size==8) {
			return CS_MONEY_TYPE;
		} else {
			fprintf(stderr,"Error! unknown money size of %d\n",size);
		}
         case SYBDATETIME:
      		return CS_DATETIME_TYPE;
		break;
         case SYBDATETIME4:
      		return CS_DATETIME4_TYPE;
		break;
	 case SYBDATETIMN:
      		if (size==4) {
			return CS_DATETIME4_TYPE;
		} else if (size==8) {
			return CS_DATETIME_TYPE;
		} else {
			fprintf(stderr,"Error! unknown date size of %d\n",size);
			return CS_FAIL;
		}
		break;
         case SYBNUMERIC:
      		return CS_NUMERIC_TYPE;
		break;
         case SYBDECIMAL:
      		return CS_DECIMAL_TYPE;
		break;
         case SYBBINARY:
      		return CS_BINARY_TYPE;
		break;
         case SYBIMAGE:
      		return CS_IMAGE_TYPE;
		break;
         case SYBVARBINARY:
      		return CS_VARBINARY_TYPE;
		break;
         case SYBTEXT:
      		return CS_TEXT_TYPE;
		break;
   }
}
int _ct_get_server_type(int datatype)
{
   tdsdump_log(TDS_DBG_FUNC, "%L inside _ct_get_server_type()\n");
   switch (datatype) {
      case CS_IMAGE_TYPE:
	 return SYBIMAGE;
	 break;
      case CS_BINARY_TYPE:
	 return SYBBINARY;
	 break;
      case CS_BIT_TYPE:
         return SYBBIT;
         break;
      case CS_CHAR_TYPE:
         return SYBCHAR;
         break;
      case CS_INT_TYPE:
         return SYBINT4;
         break;
      case CS_SMALLINT_TYPE:
         return SYBINT2;
         break;
      case CS_TINYINT_TYPE:
         return SYBINT1;
         break;
      case CS_REAL_TYPE:
         return SYBREAL;
         break;
      case CS_FLOAT_TYPE:
         return SYBFLT8;
         break;
      case CS_MONEY_TYPE:
         return SYBMONEY;
         break;
      case CS_MONEY4_TYPE:
         return SYBMONEY4;
         break;
      case CS_DATETIME_TYPE:
         return SYBDATETIME;
         break;
      case CS_DATETIME4_TYPE:
         return SYBDATETIME4;
         break;
      case CS_NUMERIC_TYPE:
         return SYBNUMERIC;
         break;
      case CS_DECIMAL_TYPE:
         return SYBDECIMAL;
         break;
      case CS_VARBINARY_TYPE:
         return SYBVARBINARY;
	 break;
      case CS_TEXT_TYPE:
         return SYBTEXT;
	 break;
      default:
         return -1;
         break;
   }
}

CS_RETCODE ct_cancel(CS_CONNECTION *conn, CS_COMMAND *cmd, CS_INT type)
{
   CS_RETCODE ret;

   tdsdump_log(TDS_DBG_FUNC, "%L inside ct_cancel()\n");
   if (type == CS_CANCEL_CURRENT) {
      if (conn || !cmd)  return CS_FAIL;
      while (1) {
         ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, NULL);
         if ((ret != CS_SUCCEED) && (ret != CS_ROW_FAIL)) {
            break;
         }
      }
      if (cmd->con->tds_socket) {
         tds_free_all_results(cmd->con->tds_socket);
      }
      return ret;
   }

   if ((conn && cmd) || (!conn && !cmd)) {
      return CS_FAIL;
   }
   if (cmd)  conn = cmd->con;
   tds_send_cancel(conn->tds_socket);
   tds_process_cancel(conn->tds_socket);
   return CS_SUCCEED;
}

CS_RETCODE ct_describe(CS_COMMAND *cmd, CS_INT item, CS_DATAFMT *datafmt)
{
TDSSOCKET *tds;
TDSRESULTINFO *resinfo;
TDSCOLINFO *curcol;

	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_describe()\n");
	tds = cmd->con->tds_socket;

	if (cmd->dynamic_cmd) {
		resinfo = tds->dyns[tds->cur_dyn_elem]->res_info;
	} else {
 		resinfo = cmd->con->tds_socket->res_info;
	}

	if (item<1 || item>resinfo->num_cols) return CS_FAIL;	
	curcol=resinfo->columns[item-1];
	strncpy(datafmt->name, curcol->column_name, CS_MAX_NAME);
	datafmt->namelen = strlen(curcol->column_name);
	/* need to turn the SYBxxx into a CS_xxx_TYPE */
	datafmt->datatype = _ct_get_client_type(curcol->column_type, curcol->column_size); 
	tdsdump_log(TDS_DBG_INFO1, "%L inside ct_describe() datafmt->datatype = %d server type %d\n", datafmt->datatype, curcol->column_type);
	datafmt->maxlength = curcol->column_size;
	datafmt->usertype = curcol->column_usertype; 
	datafmt->precision = curcol->column_prec; 
	datafmt->scale = curcol->column_scale; 
	/* FIX ME -- TDS 5.0 has status information in the results 
	** however, this will work for 4.2 as well */
	if (is_nullable_type(curcol->column_type)) 
		datafmt->status |= CS_CANBENULL;
	datafmt->count = 1;
	datafmt->locale = NULL;
	
	return CS_SUCCEED;
}
CS_RETCODE ct_res_info_dyn(CS_COMMAND *cmd, CS_INT type, CS_VOID *buffer, CS_INT buflen, CS_INT *out_len)
{
TDSSOCKET *tds = cmd->con->tds_socket;
TDSDYNAMIC *dyn;
CS_INT int_val;

	switch(type) {
		case CS_NUMDATA:
			dyn = tds->dyns[tds->cur_dyn_elem];
			int_val = dyn->res_info->num_cols;
			memcpy(buffer, &int_val, sizeof(CS_INT));
			break;
		default:
			fprintf(stderr,"Unknown type in ct_res_info_dyn: %d\n",type);
			return CS_FAIL;
	}
	return CS_SUCCEED;
}
CS_RETCODE ct_res_info(CS_COMMAND *cmd, CS_INT type, CS_VOID *buffer, CS_INT buflen, CS_INT *out_len)
{
TDSSOCKET *tds = cmd->con->tds_socket;
TDSRESULTINFO *resinfo = tds->res_info;
CS_INT int_val;

	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_res_info()\n");
	if (cmd->dynamic_cmd) {
		return ct_res_info_dyn(cmd, type, buffer, buflen, out_len);
	}
	switch(type) {
		case CS_NUMDATA:
			if (!resinfo) {
				int_val = 0;
			} else {
				int_val = resinfo->num_cols;
			}
			memcpy(buffer, &int_val, sizeof(CS_INT));
			break;
		case CS_ROW_COUNT:
			/* resinfo check by kostya@warmcat.excom.spb.su */
			if (resinfo) {
				int_val = resinfo->row_count;
			} else {
				int_val = tds->rows_affected;
			}
			memcpy(buffer, &int_val, sizeof(CS_INT));
			break;
		default:
			fprintf(stderr,"Unknown type in ct_res_info: %d\n",type);
			return CS_FAIL;
			break;
	}
	return CS_SUCCEED;
}
CS_RETCODE ct_config(CS_CONTEXT *ctx, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_config() action = %s property = %d\n",
		CS_GET ? "CS_GET" : "CS_SET", property);
	return CS_SUCCEED;
}
CS_RETCODE ct_cmd_props(CS_COMMAND *cmd, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_cmd_props() action = %s property = %d\n",
		CS_GET ? "CS_GET" : "CS_SET", property);
	return CS_SUCCEED;
}
CS_RETCODE ct_compute_info(CS_COMMAND *cmd, CS_INT type, CS_INT colnum, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_compute_info()\n");
	return CS_SUCCEED;
}
CS_RETCODE ct_get_data(CS_COMMAND *cmd, CS_INT item, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_get_data()\n");
	return CS_SUCCEED;
}
CS_RETCODE ct_send_data(CS_COMMAND *cmd, CS_VOID *buffer, CS_INT buflen)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_send_data()\n");
	return CS_SUCCEED;
}
CS_RETCODE ct_data_info(CS_COMMAND *cmd, CS_INT action, CS_INT colnum,
CS_IODESC *iodesc)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_data_info()\n");
	return CS_SUCCEED;
}
CS_RETCODE ct_capability(CS_CONNECTION *con, CS_INT action, CS_INT type, CS_INT capability, CS_VOID *value)
{
TDSLOGIN *login;
unsigned char *mask;
	
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_capability()\n");
	login = (TDSLOGIN *) con->tds_login;
	mask = login->capabilities;

	if (action==CS_SET && type==CS_CAP_RESPONSE) {
		if (*((CS_BOOL *)value)==CS_TRUE) {
			switch(capability) {
			case CS_DATA_NOBOUNDARY:
				mask[13]|=0x01;break;
			case CS_DATA_NOTDSDEBUG:
				mask[13]|=0x02;break;
			case CS_RES_NOSTRIPBLANKS:
				mask[13]|=0x04;break;
			case CS_DATA_NOINT8:
				mask[13]|=0x08;break;
			case CS_DATA_NOINTN:
				mask[14]|=0x01;break;
			case CS_DATA_NODATETIMEN:
				mask[14]|=0x02;break;
			case CS_DATA_NOMONEYN:
				mask[14]|=0x04;break;
			case CS_CON_NOOOB:
				mask[14]|=0x08;break;
			case CS_CON_NOINBAND:
				mask[14]|=0x10;break;
			case CS_PROTO_NOTEXT:
				mask[14]|=0x20;break;
			case CS_PROTO_NOBULK:
				mask[14]|=0x40;break;
			case CS_DATA_NOSENSITIVITY:
				mask[14]|=0x80;break;
			case CS_DATA_NOFLT4:
				mask[15]|=0x01;break;
			case CS_DATA_NOFLT8:
				mask[15]|=0x02;break;
			case CS_DATA_NONUM:
				mask[15]|=0x04;break;
			case CS_DATA_NOTEXT:
				mask[15]|=0x08;break;
			case CS_DATA_NOIMAGE:
				mask[15]|=0x10;break;
			case CS_DATA_NODEC:
				mask[15]|=0x20;break;
			case CS_DATA_NOLCHAR:
				mask[15]|=0x40;break;
			case CS_DATA_NOLBIN:
				mask[15]|=0x80;break;
			case CS_DATA_NOCHAR:
				mask[16]|=0x01;break;
			case CS_DATA_NOVCHAR:
				mask[16]|=0x02;break;
			case CS_DATA_NOBIN:
				mask[16]|=0x04;break;
			case CS_DATA_NOVBIN:
				mask[16]|=0x08;break;
			case CS_DATA_NOMNY8:
				mask[16]|=0x10;break;
			case CS_DATA_NOMNY4:
				mask[16]|=0x20;break;
			case CS_DATA_NODATE8:
				mask[16]|=0x40;break;
			case CS_DATA_NODATE4:
				mask[16]|=0x80;break;
			case CS_RES_NOMSG:
				mask[17]|=0x02;break;
			case CS_RES_NOEED:
				mask[17]|=0x04;break;
			case CS_RES_NOPARAM:
				mask[17]|=0x08;break;
			case CS_DATA_NOINT1:
				mask[17]|=0x10;break;
			case CS_DATA_NOINT2:
				mask[17]|=0x20;break;
			case CS_DATA_NOINT4:
				mask[17]|=0x40;break;
			case CS_DATA_NOBIT:
				mask[17]|=0x80;break;
			}
		} else {
			switch(capability) {
			case CS_DATA_NOBOUNDARY:
				mask[13]&=(!0x01);break;
			case CS_DATA_NOTDSDEBUG:
				mask[13]&=(!0x02);break;
			case CS_RES_NOSTRIPBLANKS:
				mask[13]&=(!0x04);break;
			case CS_DATA_NOINT8:
				mask[13]&=(!0x08);break;
			case CS_DATA_NOINTN:
				mask[14]&=(!0x01);break;
			case CS_DATA_NODATETIMEN:
				mask[14]&=(!0x02);break;
			case CS_DATA_NOMONEYN:
				mask[14]&=(!0x04);break;
			case CS_CON_NOOOB:
				mask[14]&=(!0x08);break;
			case CS_CON_NOINBAND:
				mask[14]&=(!0x10);break;
			case CS_PROTO_NOTEXT:
				mask[14]&=(!0x20);break;
			case CS_PROTO_NOBULK:
				mask[14]&=(!0x40);break;
			case CS_DATA_NOSENSITIVITY:
				mask[14]&=(!0x80);break;
			case CS_DATA_NOFLT4:
				mask[15]&=(!0x01);break;
			case CS_DATA_NOFLT8:
				mask[15]&=(!0x02);break;
			case CS_DATA_NONUM:
				mask[15]&=(!0x04);break;
			case CS_DATA_NOTEXT:
				mask[15]&=(!0x08);break;
			case CS_DATA_NOIMAGE:
				mask[15]&=(!0x10);break;
			case CS_DATA_NODEC:
				mask[15]&=(!0x20);break;
			case CS_DATA_NOLCHAR:
				mask[15]&=(!0x40);break;
			case CS_DATA_NOLBIN:
				mask[15]&=(!0x80);break;
			case CS_DATA_NOCHAR:
				mask[16]&=(!0x01);break;
			case CS_DATA_NOVCHAR:
				mask[16]&=(!0x02);break;
			case CS_DATA_NOBIN:
				mask[16]&=(!0x04);break;
			case CS_DATA_NOVBIN:
				mask[16]&=(!0x08);break;
			case CS_DATA_NOMNY8:
				mask[16]&=(!0x10);break;
			case CS_DATA_NOMNY4:
				mask[16]&=(!0x20);break;
			case CS_DATA_NODATE8:
				mask[16]&=(!0x40);break;
			case CS_DATA_NODATE4:
				mask[16]&=(!0x80);break;
			case CS_RES_NOMSG:
				mask[17]&=(!0x02);break;
			case CS_RES_NOEED:
				mask[17]&=(!0x04);break;
			case CS_RES_NOPARAM:
				mask[17]&=(!0x08);break;
			case CS_DATA_NOINT1:
				mask[17]&=(!0x10);break;
			case CS_DATA_NOINT2:
				mask[17]&=(!0x20);break;
			case CS_DATA_NOINT4:
				mask[17]&=(!0x40);break;
			case CS_DATA_NOBIT:
				mask[17]&=(!0x80);break;
			}
		}
	} else if (action==CS_GET && type==CS_CAP_RESPONSE) {
		switch (capability) {
		case CS_DATA_NOBOUNDARY:
			*((CS_BOOL *)value)=mask[13]&0x01 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOTDSDEBUG:
			*((CS_BOOL *)value)=mask[13]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_RES_NOSTRIPBLANKS:
			*((CS_BOOL *)value)=mask[13]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOINT8:
			*((CS_BOOL *)value)=mask[13]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOINTN:
			*((CS_BOOL *)value)=mask[14]&0x01 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NODATETIMEN:
			*((CS_BOOL *)value)=mask[14]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOMONEYN:
			*((CS_BOOL *)value)=mask[14]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_CON_NOOOB:
			*((CS_BOOL *)value)=mask[14]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_CON_NOINBAND:
			*((CS_BOOL *)value)=mask[14]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_PROTO_NOTEXT:
			*((CS_BOOL *)value)=mask[14]&0x20 ? CS_TRUE : CS_FALSE;break;
		case CS_PROTO_NOBULK:
			*((CS_BOOL *)value)=mask[14]&0x40 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOSENSITIVITY:
			*((CS_BOOL *)value)=mask[14]&0x80 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOFLT4:
			*((CS_BOOL *)value)=mask[15]&0x01 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOFLT8:
			*((CS_BOOL *)value)=mask[15]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NONUM:
			*((CS_BOOL *)value)=mask[15]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOTEXT:
			*((CS_BOOL *)value)=mask[15]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOIMAGE:
			*((CS_BOOL *)value)=mask[15]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NODEC:
			*((CS_BOOL *)value)=mask[15]&0x20 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOLCHAR:
			*((CS_BOOL *)value)=mask[15]&0x40 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOLBIN:
			*((CS_BOOL *)value)=mask[15]&0x80 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOCHAR:
			*((CS_BOOL *)value)=mask[16]&0x01 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOVCHAR:
			*((CS_BOOL *)value)=mask[16]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOBIN:
			*((CS_BOOL *)value)=mask[16]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOVBIN:
			*((CS_BOOL *)value)=mask[16]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOMNY8:
			*((CS_BOOL *)value)=mask[16]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOMNY4:
			*((CS_BOOL *)value)=mask[16]&0x20 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NODATE8:
			*((CS_BOOL *)value)=mask[16]&0x40 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NODATE4:
			*((CS_BOOL *)value)=mask[16]&0x80 ? CS_TRUE : CS_FALSE;break;
		case CS_RES_NOMSG:
			*((CS_BOOL *)value)=mask[17]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_RES_NOEED:
			*((CS_BOOL *)value)=mask[17]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_RES_NOPARAM:
			*((CS_BOOL *)value)=mask[17]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOINT1:
			*((CS_BOOL *)value)=mask[17]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOINT2:
			*((CS_BOOL *)value)=mask[17]&0x20 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOINT4:
			*((CS_BOOL *)value)=mask[17]&0x40 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NOBIT:
			*((CS_BOOL *)value)=mask[17]&0x80 ? CS_TRUE : CS_FALSE;break;
		}
	} else if (action==CS_GET && type==CS_CAP_REQUEST) {
		switch (capability) {
		case CS_PROTO_DYNPROC:
			*((CS_BOOL *)value)=mask[2]&0x01 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_FLTN:
			*((CS_BOOL *)value)=mask[2]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_BITN:
			*((CS_BOOL *)value)=mask[2]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_INT8:
			*((CS_BOOL *)value)=mask[2]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_VOID:
			*((CS_BOOL *)value)=mask[2]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_CON_INBAND:
			*((CS_BOOL *)value)=mask[3]&0x01 ? CS_TRUE : CS_FALSE;break;
		case CS_CON_LOGICAL:
			*((CS_BOOL *)value)=mask[3]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_PROTO_TEXT:
			*((CS_BOOL *)value)=mask[3]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_PROTO_BULK:
			*((CS_BOOL *)value)=mask[3]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_REQ_URGNOTIF:
			*((CS_BOOL *)value)=mask[3]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_SENSITIVITY:
			*((CS_BOOL *)value)=mask[3]&0x20 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_BOUNDARY:
			*((CS_BOOL *)value)=mask[3]&0x40 ? CS_TRUE : CS_FALSE;break;
		case CS_PROTO_DYNAMIC:
			*((CS_BOOL *)value)=mask[3]&0x80 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_MONEYN:
			*((CS_BOOL *)value)=mask[4]&0x01 ? CS_TRUE : CS_FALSE;break;
		case CS_CSR_PREV:
			*((CS_BOOL *)value)=mask[4]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_CSR_FIRST:
			*((CS_BOOL *)value)=mask[4]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_CSR_LAST:
			*((CS_BOOL *)value)=mask[4]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_CSR_ABS:
			*((CS_BOOL *)value)=mask[4]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_CSR_REL:
			*((CS_BOOL *)value)=mask[4]&0x20 ? CS_TRUE : CS_FALSE;break;
		case CS_CSR_MULTI:
			*((CS_BOOL *)value)=mask[4]&0x40 ? CS_TRUE : CS_FALSE;break;
		case CS_CON_OOB:
			*((CS_BOOL *)value)=mask[4]&0x80 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_NUM:
			*((CS_BOOL *)value)=mask[5]&0x01 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_TEXT:
			*((CS_BOOL *)value)=mask[5]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_IMAGE:
			*((CS_BOOL *)value)=mask[5]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_DEC:
			*((CS_BOOL *)value)=mask[5]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_LCHAR:
			*((CS_BOOL *)value)=mask[5]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_LBIN:
			*((CS_BOOL *)value)=mask[5]&0x20 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_INTN:
			*((CS_BOOL *)value)=mask[5]&0x40 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_DATETIMEN:
			*((CS_BOOL *)value)=mask[5]&0x80 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_BIN:
			*((CS_BOOL *)value)=mask[6]&0x01 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_VBIN:
			*((CS_BOOL *)value)=mask[6]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_MNY8:
			*((CS_BOOL *)value)=mask[6]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_MNY4:
			*((CS_BOOL *)value)=mask[6]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_DATE8:
			*((CS_BOOL *)value)=mask[6]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_DATE4:
			*((CS_BOOL *)value)=mask[6]&0x20 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_FLT4:
			*((CS_BOOL *)value)=mask[6]&0x40 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_FLT8:
			*((CS_BOOL *)value)=mask[6]&0x80 ? CS_TRUE : CS_FALSE;break;
		case CS_REQ_MSG:
			*((CS_BOOL *)value)=mask[7]&0x01 ? CS_TRUE : CS_FALSE;break;
		case CS_REQ_PARAM:
			*((CS_BOOL *)value)=mask[7]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_INT1:
			*((CS_BOOL *)value)=mask[7]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_INT2:
			*((CS_BOOL *)value)=mask[7]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_INT4:
			*((CS_BOOL *)value)=mask[7]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_BIT:
			*((CS_BOOL *)value)=mask[7]&0x20 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_CHAR:
			*((CS_BOOL *)value)=mask[7]&0x40 ? CS_TRUE : CS_FALSE;break;
		case CS_DATA_VCHAR:
			*((CS_BOOL *)value)=mask[7]&0x80 ? CS_TRUE : CS_FALSE;break;
		case CS_REQ_LANG:
			*((CS_BOOL *)value)=mask[8]&0x02 ? CS_TRUE : CS_FALSE;break;
		case CS_REQ_RPC:
			*((CS_BOOL *)value)=mask[8]&0x04 ? CS_TRUE : CS_FALSE;break;
		case CS_REQ_NOTIF:
			*((CS_BOOL *)value)=mask[8]&0x08 ? CS_TRUE : CS_FALSE;break;
		case CS_REQ_MSTMT:
			*((CS_BOOL *)value)=mask[8]&0x10 ? CS_TRUE : CS_FALSE;break;
		case CS_REQ_BCP:
			*((CS_BOOL *)value)=mask[8]&0x20 ? CS_TRUE : CS_FALSE;break;
		case CS_REQ_CURSOR:
			*((CS_BOOL *)value)=mask[8]&0x40 ? CS_TRUE : CS_FALSE;break;
		case CS_REQ_DYN:
			*((CS_BOOL *)value)=mask[8]&0x80 ? CS_TRUE : CS_FALSE;break;
			/* *((CS_BOOL *)value)=CS_FALSE; */
		}
	} else {
		/* bad values */
		return CS_FAIL;
	}
	return CS_SUCCEED;
}
CS_RETCODE ct_dynamic(CS_COMMAND *cmd, CS_INT type, CS_CHAR *id, CS_INT idlen, CS_CHAR *buffer, CS_INT buflen)
{
static int stmt_no=1;
int query_len, id_len;
TDSDYNAMIC *dyn;
TDSSOCKET *tds;
int elem;

	cmd->dynamic_cmd=type;
	switch(type) {
		case CS_PREPARE:
			/* store away the id */
		     if (idlen==CS_NULLTERM) {
				id_len = strlen(id);
        		} else {
				id_len = idlen;
			}
			if (cmd->dyn_id) free(cmd->dyn_id);
			cmd->dyn_id = (char *) malloc(id_len + 1);
			strncpy(cmd->dyn_id,(char *)id,id_len);
			cmd->dyn_id[id_len]='\0';

			/* now the query */
		        if (buflen==CS_NULLTERM) {
				query_len = strlen(buffer);
        		} else {
				query_len = buflen;
			}
			if (cmd->query) free(cmd->query);
			cmd->query = (char *) malloc(query_len + 1);
			strncpy(cmd->query,(char *)buffer,query_len);
			cmd->query[query_len]='\0';

			break;
		case CS_DEALLOC:
			break;
		case CS_DESCRIBE_INPUT:
			break;
		case CS_EXECUTE:
			/* store away the id */
		     if (idlen==CS_NULLTERM) {
				id_len = strlen(id);
        		} else {
				id_len = idlen;
			}
			if (cmd->dyn_id) free(cmd->dyn_id);
			cmd->dyn_id = (char *) malloc(id_len + 1);
			strncpy(cmd->dyn_id,(char *)id,id_len);
			cmd->dyn_id[id_len]='\0';

			/* free any input parameters */
			tds = cmd->con->tds_socket;
			elem = tds_lookup_dynamic(tds, cmd->dyn_id);
			dyn = tds->dyns[elem];
			break;
	}
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_dynamic()\n");
	return CS_SUCCEED;
}
CS_RETCODE ct_param(CS_COMMAND *cmd, CS_DATAFMT *datafmt, CS_VOID *data, CS_INT datalen, CS_SMALLINT indicator)
{
TDSSOCKET *tds;
TDSDYNAMIC *dyn;
TDSINPUTPARAM *param;
int elem;

	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_param()\n");
	tdsdump_log(TDS_DBG_INFO1, "%L ct_param() data addr = %d data length = %d\n", data, datalen);

	tds = cmd->con->tds_socket;

	elem = tds_lookup_dynamic(tds, cmd->dyn_id);
	dyn = tds->dyns[elem];
	param = tds_add_input_param(dyn);
	param->column_type = _ct_get_server_type(datafmt->datatype);
	param->varaddr = data;
	if (datalen==CS_NULLTERM) {
		param->column_bindlen = 0;
	} else {
		param->column_bindlen = datalen;
	}
	param->is_null = indicator;

	return CS_SUCCEED;
}
CS_RETCODE ct_options(CS_CONNECTION *con, CS_INT action, CS_INT option, CS_VOID *param, CS_INT paramlen, CS_INT *outlen)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside ct_options() action = %s option = %d\n",
		CS_GET ? "CS_GET" : "CS_SET", option);
	return CS_SUCCEED;
}
