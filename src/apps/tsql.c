/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2002 Brian Bruns
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

#include <stdio.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif
#include <string.h>
#include "tds.h"
#include "tdsconvert.h"

#ifndef HAVE_READLINE
char *readline(char *prompt)
{
char *buf, line[1000];
int i = 0;

	printf("%s",prompt);
	fgets(line,1000,stdin);
	for (i=strlen(line);i>0;i--) {
		if (line[i]=='\n') {
			line[i]='\0';
			break;
		}
	}
	buf = (char *) malloc(strlen(line)+1);
	strcpy(buf,line);

	return buf;
}
add_history(char *s)
{
}
#endif

int do_query(TDSSOCKET *tds, char *buf)
{
int rc, i;
TDSCOLINFO *col;
int ctype;
CONV_RESULT dres;

	rc = tds_submit_query(tds,buf);
	if (rc != TDS_SUCCEED) {
		fprintf(stderr, "tds_submit_query() failed\n");
		return 1;
	}

	while ((rc=tds_process_result_tokens(tds))==TDS_SUCCEED) {
		if (tds->res_info) {
			for (i=0; i<tds->res_info->num_cols; i++) {
				fprintf(stdout, "%s\t", tds->res_info->columns[i]->column_name);
			}
			fprintf(stdout,"\n");
		}
		while ((rc=tds_process_row_tokens(tds))==TDS_SUCCEED) {

			if (!tds->res_info) 
				continue;

			for (i=0; i<tds->res_info->num_cols; i++) {
				col = tds->res_info->columns[i];
				ctype = tds_get_conversion_type(col->column_type, col->column_size);

                    if(tds_convert(tds->tds_ctx,
 					ctype,
 					&tds->res_info->current_row[col->column_offset],
 					col->column_size,
					SYBVARCHAR,
					255,
					&dres) == TDS_FAIL)
			    continue;
				fprintf(stdout,"%s\t",dres.c);
				free(dres.c);
			}
			fprintf(stdout,"\n");
         }
	}
	return 0;
}

void print_usage(char *progname)
{
			fprintf(stderr,"Usage: %s [-S <server> | -H <hostname> -p <port>] -U <username> [ -P <password> ]\n",progname);
}
int populate_login(TDSLOGIN *login, int argc, char **argv)
{
char *hostname = NULL;
char *servername = NULL;
char *username = NULL;
char *password = NULL;
int  port = 0;
int  opt;

     while ((opt=getopt(argc, argv, "H:S:V::P:U:p:v"))!=-1) {
          switch (opt) {
          case 'H':
               hostname = (char *) malloc(strlen(optarg)+1);
               strcpy(hostname, optarg);
          break;
          case 'S':
               servername = (char *) malloc(strlen(optarg)+1);
               strcpy(servername, optarg);
          break;
          case 'U':
               username = (char *) malloc(strlen(optarg)+1);
               strcpy(username, optarg);
          break;
          case 'P':
               password = (char *) malloc(strlen(optarg)+1);
               strcpy(password, optarg);
          break;
          case 'p':
			port = atoi(optarg);
		break;
          default:
			print_usage(argv[0]);
			exit(1);
          break;
          }
     }

	/* validate parameters */
	if (!servername && !hostname) {
			fprintf(stderr,"Missing argument -S or -H\n");
			print_usage(argv[0]);
			exit(1);
	}
	if (hostname && !port) {
			fprintf(stderr,"Missing argument -p \n");
			print_usage(argv[0]);
			exit(1);
	}
	if (!username) { 
			fprintf(stderr,"Missing argument -U \n");
			print_usage(argv[0]);
			exit(1);
	}
	if (!servername && !hostname) {
			print_usage(argv[0]);
			exit(1);
	}
	if (!password) {
			char *tmp = getpass("Password: ");
			password = strdup(tmp);
	}

	/* all validated, let's do it */

	/* if it's a servername */
	if (servername) {
		tds_set_user(login, username);
		tds_set_app(login, "TSQL");
		tds_set_host(login, "myhost");
		tds_set_library(login, "TDS-Library");
		tds_set_server(login, servername);
		tds_set_charset(login, "iso_1");
		tds_set_language(login, "us_english");
		tds_set_packet(login, 512);
		tds_set_passwd(login, password);
	/* else we specified hostname/port */
	} else {
		tds_set_user(login, username);
		tds_set_app(login, "TSQL");
		tds_set_host(login, "myhost");
		tds_set_library(login, "TDS-Library");
		tds_set_server(login, hostname);
		tds_set_port(login, port);
		tds_set_charset(login, "iso_1");
		tds_set_language(login, "us_english");
		tds_set_packet(login, 512);
		tds_set_passwd(login, password);
	}

	/* free up all the memory */
	if (hostname) free(hostname);
	if (username) free(username);
	if (password) free(password);
	if (servername) free(servername);
}
int tsql_handle_message(void *ctxptr, void *tdsptr, void *msgptr)
{
	TDSCONTEXT *context = (TDSCONTEXT *) ctxptr;
	TDSSOCKET *tds = (TDSSOCKET *) tdsptr;
	TDSMSGINFO *msg = (TDSMSGINFO *) msgptr;

     if( msg->msg_number > 0  && msg->msg_number != 5701) {
		fprintf (stderr, "Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n",
                         msg->msg_number,
                         msg->msg_level,
                         msg->msg_state,
                         msg->server,
                         msg->line_number,
                         msg->message);
	}
	tds_reset_msg_info(msg);

	return 1;
}

int 
main(int argc, char **argv)
{
char *s;
char prompt[20];
int line = 1;
char *mybuf;
int bufsz = 4096;
int done = 0;
TDSSOCKET *tds;
TDSLOGIN *login;
TDSCONTEXT *context;

	/* grab a login structure */
	login = (void *) tds_alloc_login();

	context = tds_alloc_context();
	if( context->locale && !context->locale->date_fmt ) {
		/* set default in case there's no locale file */
		context->locale->date_fmt = strdup("%b %e %Y %l:%M%p");
	}

	context->msg_handler = tsql_handle_message;
	context->err_handler = tsql_handle_message;

	/* process all the command line args into the login structure */
	populate_login(login, argc, argv);

	/* Try to open a connection*/
	tds = tds_connect(login, context, NULL); 

	if (!tds) {
		/* FIX ME -- need to hook up message/error handlers */
		fprintf(stderr, "There was a problem connecting to the server\n");
		exit(1);
	}
	/* give the buffer an initial size */
	bufsz = 4096;
	mybuf = (char *) malloc(bufsz);
	mybuf[0]='\0';

	sprintf(prompt,"1> ");
	s=readline(prompt);
	if (!strcmp(s,"exit") || !strcmp(s,"quit") || !strcmp(s,"bye")) {
		done = 1;
	}
	while (!done) {
		if (!strcmp(s,"go")) {
			line = 0;
			do_query(tds, mybuf);
			mybuf[0]='\0';
		} else if (!strcmp(s,"reset")) {
			line = 0;
			mybuf[0]='\0';
		} else {
			while (strlen(mybuf) + strlen(s) > bufsz) {
				bufsz *= 2;
				mybuf = (char *) realloc(mybuf, bufsz);
			}
			add_history(s);
			strcat(mybuf,s);
			/* preserve line numbering for the parser */
			strcat(mybuf,"\n");
		}
		sprintf(prompt,"%d> ",++line);
		free(s);
		s=readline(prompt);
		if (!strcmp(s,"exit") || !strcmp(s,"quit") || !strcmp(s,"bye")) {
			done = 1;
		}
	}

	/* close up shop */
	tds_free_socket(tds);
	tds_free_login(login);
	tds_free_context(context);

	return 0;
}
