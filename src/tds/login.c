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
#include "tds.h"
#include "tdsutil.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef WIN32                                           
#define IOCTL(a,b,c) ioctlsocket(a, b, c)
#else
#define IOCTL(a,b,c) ioctl(a, b, c)
#endif

static char  software_version[]   = "$Id: login.c,v 1.25 2002-07-15 03:29:58 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

extern int g__numeric_bytes_per_prec[39];


void tds_set_version(TDSLOGIN *tds_login, short major_ver, short minor_ver)
{
	tds_login->major_version=major_ver;
	tds_login->minor_version=minor_ver;
}
void tds_set_packet(TDSLOGIN *tds_login, short packet_size)
{ 
	tds_login->block_size=packet_size; 
}
void tds_set_port(TDSLOGIN *tds_login, int port)
{ 
	tds_login->port=port; 
}
void tds_set_passwd(TDSLOGIN *tds_login, char *password)
{ 
	if (password) {
		strncpy(tds_login->password, password, TDS_MAX_LOGIN_STR_SZ); 
	}
}
void tds_set_bulk(TDSLOGIN *tds_login, TDS_TINYINT enabled)
{
	tds_login->bulk_copy = enabled ? 0 : 1;
}
void tds_set_user(TDSLOGIN *tds_login, char *username)
{ 
	strncpy(tds_login->user_name, username, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_host(TDSLOGIN *tds_login, char *hostname)
{
	strncpy(tds_login->host_name, hostname, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_app(TDSLOGIN *tds_login, char *application)
{
	strncpy(tds_login->app_name, application, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_server(TDSLOGIN *tds_login, char *server)
{
	if(!server || strlen(server) == 0) {
		server = getenv("DSQUERY");
		if(!server || strlen(server) == 0) {
			server = "SYBASE";
		}
	}
	strncpy(tds_login->server_name, server, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_library(TDSLOGIN *tds_login, char *library)
{
	strncpy(tds_login->library, library, 10);
}
void tds_set_charset(TDSLOGIN *tds_login, char *charset)
{
	strncpy(tds_login->char_set, charset, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_language(TDSLOGIN *tds_login, char *language)
{
	strncpy(tds_login->language, language, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_timeouts(TDSLOGIN *tds_login, int connect, int query, int longquery)                   /* Jeffs' hack to support timeouts */
{
	tds_login->connect_timeout = connect;
	tds_login->query_timeout = query;
	tds_login->longquery_timeout = longquery;
}
void tds_set_longquery_handler(TDSLOGIN * tds_login, void (*longquery_func)(long), long longquery_param) /* Jeff's hack */
{
	tds_login->longquery_func = longquery_func;
	tds_login->longquery_param = longquery_param;
}
extern void tds_set_capabilities(TDSLOGIN *tds_login, unsigned char *capabilities, int size)
{
	memcpy(tds_login->capabilities, capabilities, 
		size > TDS_MAX_CAPABILITY ? TDS_MAX_CAPABILITY : size);
}

TDSSOCKET *tds_connect(TDSLOGIN *login, TDSCONTEXT *context, void *parent) 
{
TDSSOCKET	*tds;
struct sockaddr_in      sin;
/* Jeff's hack - begin */
unsigned long ioctl_blocking = 1;                      
struct timeval selecttimeout;                         
fd_set fds;                                          
int retval;                                         
time_t start, now;
TDSCONFIGINFO *config;
/* 13 + max string of 32bit int, 30 should cover it */
char query[30];
char *tmpstr;
int connect_timeout = 0;


FD_ZERO (&fds);                                    
/* end */

	config = tds_get_config(NULL, login, context->locale);

	/*
	** If a dump file has been specified, start logging
	*/
	if (config->dump_file) {
   		tdsdump_open(config->dump_file);
	}

	/* 
	** The initial login packet must have a block size of 512.
	** Once the connection is established the block size can be changed
	** by the server with TDS_ENV_CHG_TOKEN
	*/
	tds = tds_alloc_socket(context, 512);
	tds_set_parent(tds, parent);

	tds->major_version=config->major_version;
	tds->minor_version=config->minor_version;
	tds->emul_little_endian=config->emul_little_endian;
#ifdef WORDS_BIGENDIAN
	if (IS_TDS70(tds) || IS_TDS80(tds)) {
		/* TDS 7/8 only supports little endian */
		tds->emul_little_endian=1;
	}
#endif

	/* set up iconv */
	if (config->client_charset) {
		tds_iconv_open(tds, config->client_charset);
	}

	/* specified a date format? */
	/*
	if (config->date_fmt) {
		tds->date_fmt=strdup(config->date_fmt);
	}
	*/
	if (login->connect_timeout) {
		connect_timeout = login->connect_timeout;
	} else if (config->connect_timeout) {
		connect_timeout = config->connect_timeout;
	}

	/* Jeff's hack - begin */
	tds->timeout = (connect_timeout) ? login->query_timeout : 0;        
	tds->longquery_timeout = (connect_timeout) ? login->longquery_timeout : 0;
	tds->longquery_func = login->longquery_func;
	tds->longquery_param = login->longquery_param;
	/* end */

	/* verify that ip_addr is not NULL */
	if (!config->ip_addr) {
		tdsdump_log(TDS_DBG_ERROR, "%L IP address pointer is NULL\n");
		if (config->server_name) {
			tmpstr = malloc(strlen(config->server_name)+100);
			sprintf(tmpstr,"Server %s not found!",config->server_name);
			tds_client_msg(tds->tds_ctx, tds, 10019, 9, 0, 0, tmpstr);
			free(tmpstr);
		} else {
			tds_client_msg(tds->tds_ctx, tds, 10020, 9, 0, 0, "No server specified!");
		}
		tds_free_config(config);
		tds_free_socket(tds);
		return NULL;
	}
	sin.sin_addr.s_addr = inet_addr(config->ip_addr);
	if (sin.sin_addr.s_addr == -1) {
		tdsdump_log(TDS_DBG_ERROR, "%L inet_addr() failed, IP = %s\n", config->ip_addr);
		tds_free_config(config);
		tds_free_socket(tds);
		return NULL;
	}

       	sin.sin_family = AF_INET;
       	sin.sin_port = htons(config->port);

	memcpy(tds->capabilities,login->capabilities,TDS_MAX_CAPABILITY);

	tdsdump_log(TDS_DBG_INFO1, "%L Connecting addr %s port %d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
        if ((tds->s = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
                perror ("socket");
		tds_free_config(config);
		tds_free_socket(tds);
		return NULL;
        }

    /* Jeff's hack *** START OF NEW CODE *** */
	if (connect_timeout) {
		start = time (NULL);
		ioctl_blocking = 1; /* ~0; //TRUE; */
		if (IOCTL(tds->s, FIONBIO, &ioctl_blocking) < 0) {
			tds_free_config(config);
			tds_free_socket(tds);
			return NULL;
		}
		retval = connect(tds->s, (struct sockaddr *) &sin, sizeof(sin));
		if (retval < 0 && errno == EINPROGRESS) retval = 0;
		if (retval < 0) {
			perror("src/tds/login.c: tds_connect (timed)");
			tds_free_config(config);
			tds_free_socket(tds);
			return NULL;
		}
          /* Select on writeability for connect_timeout */
		selecttimeout.tv_sec = 0;
		selecttimeout.tv_usec = 0;

		FD_SET (tds->s, &fds);
		retval = select(tds->s + 1, NULL, &fds, NULL, &selecttimeout);
		/* patch from Kostya Ivanov <kostya@warmcat.excom.spb.su> */
		if (retval < 0 && errno == EINTR)
			retval = 0;
		/* end patch */

		now = time (NULL);

		while ((retval == 0) && ((now-start) < connect_timeout)) {
			tds_msleep(1);
			FD_SET (tds->s, &fds);
			selecttimeout.tv_sec = 0;
			selecttimeout.tv_usec = 0;
			retval = select(tds->s + 1, NULL, &fds, NULL, &selecttimeout);
			now = time (NULL);
    		}

		if ((now-start) > connect_timeout) {
			tds_free_config(config);
			tds_free_socket(tds);
			return NULL;
		}
	} else {
        if (connect(tds->s, (struct sockaddr *) &sin, sizeof(sin)) <0) {
		char *message = malloc(128);
		sprintf(message, "src/tds/login.c: tds_connect: %s:%d",
			inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		perror(message);
		free(message);
		tds_free_config(config);
		tds_free_socket(tds);
		return NULL;
        }
	}
	/* END OF NEW CODE */

	if (IS_TDS7_PLUS(tds)) {
		tds->out_flag=0x10;
		tds7_send_login(tds,config);	
	} else {
		tds->out_flag=0x02;
		tds_send_login(tds,config);	
	}
	if (!tds_process_login_tokens(tds)) {
		tds_free_socket(tds);
		tds = NULL;
		return NULL;
	}
	if (tds && config->text_size) {
		sprintf(query,"set textsize %d", config->text_size);
   		retval = tds_submit_query(tds,query);
   		if (retval == TDS_SUCCEED) {
   			while (tds_process_result_tokens(tds)==TDS_SUCCEED);
   		}
	}
    if (IS_TDS7_PLUS(tds)) {
       g__numeric_bytes_per_prec[ 0] = -1;
       g__numeric_bytes_per_prec[ 1] = 5;
       g__numeric_bytes_per_prec[ 2] = 5;
       g__numeric_bytes_per_prec[ 3] = 5;
       g__numeric_bytes_per_prec[ 4] = 5;
       g__numeric_bytes_per_prec[ 5] = 5;
       g__numeric_bytes_per_prec[ 6] = 5;
       g__numeric_bytes_per_prec[ 7] = 5;
       g__numeric_bytes_per_prec[ 8] = 5;
       g__numeric_bytes_per_prec[ 9] = 5;
       g__numeric_bytes_per_prec[10] = 9;
       g__numeric_bytes_per_prec[11] = 9;
       g__numeric_bytes_per_prec[12] = 9;
       g__numeric_bytes_per_prec[13] = 9;
       g__numeric_bytes_per_prec[14] = 9;
       g__numeric_bytes_per_prec[15] = 9;
       g__numeric_bytes_per_prec[16] = 9;
       g__numeric_bytes_per_prec[17] = 9;
       g__numeric_bytes_per_prec[18] = 9;
       g__numeric_bytes_per_prec[19] = 9;
       g__numeric_bytes_per_prec[20] = 13;
       g__numeric_bytes_per_prec[21] = 13;
       g__numeric_bytes_per_prec[22] = 13;
       g__numeric_bytes_per_prec[23] = 13;
       g__numeric_bytes_per_prec[24] = 13;
       g__numeric_bytes_per_prec[25] = 13;
       g__numeric_bytes_per_prec[26] = 13;
       g__numeric_bytes_per_prec[27] = 13;
       g__numeric_bytes_per_prec[28] = 13;
       g__numeric_bytes_per_prec[29] = 17;
       g__numeric_bytes_per_prec[30] = 17;
       g__numeric_bytes_per_prec[31] = 17;
       g__numeric_bytes_per_prec[32] = 17;
       g__numeric_bytes_per_prec[33] = 17;
       g__numeric_bytes_per_prec[34] = 17;
       g__numeric_bytes_per_prec[35] = 17;
       g__numeric_bytes_per_prec[36] = 17;
       g__numeric_bytes_per_prec[37] = 17;
       g__numeric_bytes_per_prec[38] = 17;
    }
    else {
       g__numeric_bytes_per_prec[ 0] = -1;
       g__numeric_bytes_per_prec[ 1] = 2;
       g__numeric_bytes_per_prec[ 2] = 2;
       g__numeric_bytes_per_prec[ 3] = 3;
       g__numeric_bytes_per_prec[ 4] = 3;
       g__numeric_bytes_per_prec[ 5] = 4;
       g__numeric_bytes_per_prec[ 6] = 4;
       g__numeric_bytes_per_prec[ 7] = 4;
       g__numeric_bytes_per_prec[ 8] = 5;
       g__numeric_bytes_per_prec[ 9] = 5;
       g__numeric_bytes_per_prec[10] = 6;
       g__numeric_bytes_per_prec[11] = 6;
       g__numeric_bytes_per_prec[12] = 6;
       g__numeric_bytes_per_prec[13] = 7;
       g__numeric_bytes_per_prec[14] = 7;
       g__numeric_bytes_per_prec[15] = 8;
       g__numeric_bytes_per_prec[16] = 8;
       g__numeric_bytes_per_prec[17] = 9;
       g__numeric_bytes_per_prec[18] = 9;
       g__numeric_bytes_per_prec[19] = 9;
       g__numeric_bytes_per_prec[20] = 10;
       g__numeric_bytes_per_prec[21] = 10;
       g__numeric_bytes_per_prec[22] = 11;
       g__numeric_bytes_per_prec[23] = 11;
       g__numeric_bytes_per_prec[24] = 11;
       g__numeric_bytes_per_prec[25] = 12;
       g__numeric_bytes_per_prec[26] = 12;
       g__numeric_bytes_per_prec[27] = 13;
       g__numeric_bytes_per_prec[28] = 13;
       g__numeric_bytes_per_prec[29] = 14;
       g__numeric_bytes_per_prec[30] = 14;
       g__numeric_bytes_per_prec[31] = 14;
       g__numeric_bytes_per_prec[32] = 15;
       g__numeric_bytes_per_prec[33] = 15;
       g__numeric_bytes_per_prec[34] = 16;
       g__numeric_bytes_per_prec[35] = 16;
       g__numeric_bytes_per_prec[36] = 16;
       g__numeric_bytes_per_prec[37] = 17;
       g__numeric_bytes_per_prec[38] = 17;
    }

	tds_free_config(config);
	return tds;
}
int tds_send_login(TDSSOCKET *tds, TDSCONFIGINFO *config)
{	
   char *tmpbuf;
   int tmplen;
   unsigned char be1[]= {0x02,0x00,0x06,0x04,0x08,0x01};
   unsigned char le1[]= {0x03,0x01,0x06,0x0a,0x09,0x01};
   unsigned char magic2[]={0x00,0x00};
   
   unsigned char magic3[]= {0x00,0x00,0x00};
   
/* these seem to endian flags as well 13,17 on intel/alpha 12,16 on power */
   
   unsigned char be2[]= {0x00,12,16};
   unsigned char le2[]= {0x00,13,17};
   
   /* 
   ** the former byte 0 of magic5 causes the language token and message to be 
   ** absent from the login acknowledgement if set to 1. There must be a way 
   ** of setting this in the client layer, but I am not aware of any thing of
   ** the sort -- bsb 01/17/99
   */
   unsigned char magic5[]= {0x00,0x00};
   unsigned char magic6[]= {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
   unsigned char magic7=   0x01;
   
   unsigned char magic42[]= {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
   unsigned char magic50[]= {0x00,0x00,0x00,0x00};
/*
** capabilities are now part of the tds structure.
   unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x0A,104,0x00,0x00,0x00};
*/
/*
** This is the original capabilities packet we were working with (sqsh)
   unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x0A,104,0x00,0x00,0x00};
** original with 4.x messages
   unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x0D};
** This is isql 11.0.3
   unsigned char capabilities[]= {0x01,0x07,0x00,96, 129,207, 0xFF,0xFE,62,  0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x0D};
** like isql but with 5.0 messages
   unsigned char capabilities[]= {0x01,0x07,0x00,96, 129,207, 0xFF,0xFE,62,  0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x00};
**
*/
   
   unsigned char protocol_version[4];
   unsigned char program_version[4];
   
   int rc;
   char blockstr[10], passwdstr[255];
   
   if (IS_TDS42(tds)) {
      memcpy(protocol_version,"\004\002\000\000",4);
      memcpy(program_version,"\004\002\000\000",4);
   } else if (IS_TDS46(tds)) {
      memcpy(protocol_version,"\004\006\000\000",4);
      memcpy(program_version,"\004\002\000\000",4);
   } else if (IS_TDS50(tds)) {
      memcpy(protocol_version,"\005\000\000\000",4);
      memcpy(program_version,"\005\000\000\000",4);
   } else {
      fprintf(stderr,"Unknown protocol version!");
      exit(1);
   }
   /*
   ** the following code is adapted from  Arno Pedusaar's 
   ** (psaar@fenar.ee) MS-SQL Client. His was a much better way to
   ** do this, (well...mine was a kludge actually) so here's mostly his
   */
   
   rc=tds_put_string(tds,config->host_name,TDS_MAX_LOGIN_STR_SZ);   /* client host name */
   rc|=tds_put_string(tds,config->user_name,TDS_MAX_LOGIN_STR_SZ);  /* account name */
   rc|=tds_put_string(tds,config->password,TDS_MAX_LOGIN_STR_SZ);  /* account password */
   rc|=tds_put_string(tds,"37876",TDS_MAX_LOGIN_STR_SZ);        /* host process */
#ifdef WORDS_BIGENDIAN
   if (tds->emul_little_endian) {
      rc|=tds_put_n(tds,le1,6);
   } else {
      rc|=tds_put_n(tds,be1,6);
   }
#else
   rc|=tds_put_n(tds,le1,6);
#endif
   rc|=tds_put_byte(tds,config->bulk_copy);
   rc|=tds_put_n(tds,magic2,2);
   if (IS_TDS42(tds)) {
      rc|=tds_put_int(tds,512);
   } else {
      rc|=tds_put_int(tds,0);
   }
   rc|=tds_put_n(tds,magic3,3);
   rc|=tds_put_string(tds,config->app_name,TDS_MAX_LOGIN_STR_SZ);
   rc|=tds_put_string(tds,config->server_name,TDS_MAX_LOGIN_STR_SZ);
   if (IS_TDS42(tds)) {
      rc|=tds_put_string(tds,config->password,255);
   } else {
	 if(config->password == NULL) {
		sprintf(passwdstr, "%c%c%s", 0, 0, "");
      	rc|=tds_put_buf(tds,passwdstr,255,(unsigned char)2);
	 } else {
      	sprintf(passwdstr,"%c%c%s",0,
			(unsigned char)strlen(config->password),
			config->password);
      	rc|=tds_put_buf(tds,passwdstr,255,(unsigned char)strlen(config->password)+2);
	 }
   }
   
   rc|=tds_put_n(tds,protocol_version,4); /* TDS version; { 0x04,0x02,0x00,0x00 } */
   rc|=tds_put_string(tds,config->library,10);  /* client program name */
   if (IS_TDS42(tds)) { 
      rc|=tds_put_int(tds,0);
   } else {
      rc|=tds_put_n(tds,program_version,4); /* program version ? */
   }
#ifdef WORDS_BIGENDIAN
   if (tds->emul_little_endian) {
      rc|=tds_put_n(tds,le2,3);
   } else {
      rc|=tds_put_n(tds,be2,3);
   }
#else
   rc|=tds_put_n(tds,le2,3);
#endif
   rc|=tds_put_string(tds,config->language,TDS_MAX_LOGIN_STR_SZ);  /* language */
   rc|=tds_put_byte(tds,config->suppress_language);
   rc|=tds_put_n(tds,magic5,2);
   rc|=tds_put_byte(tds,config->encrypted);
   rc|=tds_put_n(tds,magic6,10);
   rc|=tds_put_string(tds,config->char_set,TDS_MAX_LOGIN_STR_SZ);  /* charset */
   rc|=tds_put_byte(tds,magic7);
   sprintf(blockstr,"%d",config->block_size);
   rc|=tds_put_string(tds,blockstr,6); /* network packet size */
   if (IS_TDS42(tds)) {
      rc|=tds_put_n(tds,magic42,8);
   } else if (IS_TDS46(tds)) {
      rc|=tds_put_n(tds,magic42,4);
   } else if (IS_TDS50(tds)) {
      rc|=tds_put_n(tds,magic50,4);
      rc|=tds_put_byte(tds, TDS_CAP_TOKEN);
      rc|=tds_put_smallint(tds, 18);
      rc|=tds_put_n(tds,tds->capabilities,TDS_MAX_CAPABILITY);
   }
   
/*
   tmpbuf = malloc(tds->out_pos);
   tmplen = tds->out_pos;
   memcpy(tmpbuf, tds->out_buf, tmplen);
   tdsdump_off();
*/
   rc|=tds_flush_packet(tds);
/*
   tdsdump_on();
   tdsdump_log(TDS_DBG_NETWORK, "Sending packet (passwords supressed)@ %L\n%D\n", tmpbuf, tmplen);
   free(tmpbuf);
*/
   /* get_incoming(tds->s); */
	return 0;
}

/*
** tds7_send_login() -- Send a TDS 7.0 login packet
** TDS 7.0 login packet is vastly different and so gets its own function
*/
int tds7_send_login(TDSSOCKET *tds, TDSCONFIGINFO *config)
{	
int rc;
unsigned char magic1[] = 
	{6,0x83,0xf2,0xf8,  /* Client Program version */
     0xff,0x0,0x0,0x0,  /* Client PID */
     0x0,0xe0,0x03,0x0, /* Connection ID of the Primary Server (?) */
     0x0,               /* Option Flags 1 */
     0x88,              /* Option Flags 2 */
     0xff,              /* Type Flags     */
     0xff,              /* reserved Flags */
     0xff,0x36,0x04,0x00,
     0x00};
/* also seen
	{6,0x7d,0x0f,0xfd,0xff,0x0,0x0,0x0,0x0,
	0xe0,0x83,0x0,0x0,0x68,0x01,0x00,0x00,
	0x09,0x04,0x00,0x00};
*/
unsigned char magic2[] = {0x00,0x40,0x33,0x9a,0x6b,0x50};
/* 0xb4,0x00,0x30,0x00,0xe4,0x00,0x00,0x00; */
unsigned char magic3[] = "NTLMSSP";
unsigned char unicode_string[255];
int packet_size;
int size1;
int current_pos;
int domain_login = 0;

int user_name_len = config->user_name ? strlen(config->user_name) : 0;
int host_name_len = config->host_name ? strlen(config->host_name) : 0;
int app_name_len = config->app_name ? strlen(config->app_name) : 0;
int password_len = config->password ? strlen(config->password) : 0;
int server_name_len = config->server_name ? strlen(config->server_name) : 0;
int library_len = config->library ? strlen(config->library) : 0;
int language_len = config->language ? strlen(config->language) : 0;

   if (!domain_login) {
   	size1 = 86 + (
			host_name_len +
			user_name_len +
			app_name_len +
			password_len +
			server_name_len +
			library_len +
			language_len)*2;
   } else {
   	size1 = 86 + (
			host_name_len +
			app_name_len +
			server_name_len +
			library_len +
			language_len)*2;
   }
   packet_size = size1 + 48;
   tds_put_smallint(tds,packet_size);
   tds_put_n(tds,NULL,5);
   if (IS_TDS80(tds)) {
      tds_put_byte(tds,0x80);
   } else {
      tds_put_byte(tds,0x70);
   }
   tds_put_n(tds,NULL,3);       /* rest of TDSVersion which is a 4 byte field    */
   tds_put_n(tds,NULL,4);       /* desired packet size being requested by client */
   tds_put_n(tds,magic1,21);

   current_pos = 86; /* ? */
   /* host name */
   tds_put_smallint(tds,current_pos); 
   tds_put_smallint(tds,host_name_len);
   current_pos += host_name_len * 2;
   if (!domain_login) {
   	/* username */
   	tds_put_smallint(tds,current_pos);
   	tds_put_smallint(tds,user_name_len);
   	current_pos += user_name_len * 2;
   	/* password */
   	tds_put_smallint(tds,current_pos);
   	tds_put_smallint(tds,password_len);
   	current_pos += password_len * 2;
   } else {
	tds_put_smallint(tds,0);
	tds_put_smallint(tds,0);
	tds_put_smallint(tds,0);
	tds_put_smallint(tds,0);
   }
   /* app name */
   tds_put_smallint(tds,current_pos);
   tds_put_smallint(tds,app_name_len);
   current_pos += app_name_len * 2;
   /* server name */
   tds_put_smallint(tds,current_pos);
   tds_put_smallint(tds,server_name_len);
   current_pos += server_name_len * 2;
   /* unknown */
   tds_put_smallint(tds,0); 
   tds_put_smallint(tds,0); 
   /* library name */
   tds_put_smallint(tds,current_pos);
   tds_put_smallint(tds,library_len);
   current_pos += library_len * 2;
   /* language  - kostya@warmcat.excom.spb.su */
   tds_put_smallint(tds,current_pos); 
   tds_put_smallint(tds,language_len);
   current_pos += language_len * 2;
   /* database name */
   tds_put_smallint(tds,current_pos); 
   tds_put_smallint(tds,0); 

   tds_put_n(tds,magic2,6);
   tds_put_smallint(tds, size1);
   tds_put_smallint(tds, 0x30); /* this matches numbers at end of packet */
   tds_put_smallint(tds, packet_size);
   tds_put_smallint(tds, 0); 

   tds7_ascii2unicode(tds,config->host_name, unicode_string, 255);
   tds_put_n(tds,unicode_string,host_name_len * 2);
   if (!domain_login) {
   	tds7_ascii2unicode(tds,config->user_name, unicode_string, 255);
   	tds_put_n(tds,unicode_string,user_name_len * 2);
   	tds7_ascii2unicode(tds,config->password, unicode_string, 255);
   	tds7_crypt_pass(unicode_string, password_len * 2, unicode_string);
   	tds_put_n(tds,unicode_string,password_len * 2);
   }
   tds7_ascii2unicode(tds,config->app_name, unicode_string, 255);
   tds_put_n(tds,unicode_string,app_name_len * 2);
   tds7_ascii2unicode(tds,config->server_name, unicode_string, 255);
   tds_put_n(tds,unicode_string,server_name_len * 2);
   tds7_ascii2unicode(tds,config->library, unicode_string, 255);
   tds_put_n(tds,unicode_string,library_len * 2);
   tds7_ascii2unicode(tds,config->language, unicode_string, 255);
   tds_put_n(tds,unicode_string,language_len * 2);

   /* from here to the end of the packet is totally unknown */
   tds_put_n(tds,magic3,7);
   /* next two bytes -- possibly version number (NTLMSSP v1) */
   tds_put_byte(tds,0);
   tds_put_byte(tds,1);
   tds_put_n(tds,NULL,3);
   tds_put_byte(tds,6);
   tds_put_byte(tds,130);
   tds_put_n(tds,NULL,22);
   tds_put_byte(tds,48);
   tds_put_n(tds,NULL,7);
   tds_put_byte(tds,48);
   tds_put_n(tds,NULL,3);

   tdsdump_off();
   rc|=tds_flush_packet(tds);
   tdsdump_on();
   
	return 0;
}

/*
** tds7_crypt_pass() -- 'encrypt' TDS 7.0 style passwords.
** the calling function is responsible for ensuring crypt_pass is at least 
** 'len' characters
*/
unsigned char *
tds7_crypt_pass(const unsigned char *clear_pass, int len, unsigned char *crypt_pass)
{
int i;
unsigned char xormask = 0x5A;
unsigned char hi_nibble,lo_nibble ;

	for (i=0;i<len;i++) {
		lo_nibble  = (clear_pass[i] ^ xormask) >> 4;
		hi_nibble  = (clear_pass[i] ^ xormask) << 4;
		crypt_pass[i] = hi_nibble | lo_nibble;
	}
	return crypt_pass;
}
