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
#include <unistd.h>

static char  software_version[]   = "$Id: login.c,v 1.4 2002-06-24 23:29:07 jklowden Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

unsigned char *
tds7_decrypt_pass (const unsigned char *crypt_pass, int len,unsigned char *clear_pass) 
{
int i;
const unsigned char xormask=0x5A;
unsigned char hi_nibble,lo_nibble ;
	for(i=0;i<len;i++) {
		lo_nibble=(crypt_pass[i] << 4) ^ (xormask & 0xF0);
		hi_nibble=(crypt_pass[i] >> 4) ^ (xormask & 0x0F);
		clear_pass[i]=hi_nibble | lo_nibble;
	}
	return clear_pass;
}

TDSSOCKET *tds_listen(int ip_port) 
{
TDSSOCKET	*tds;
struct sockaddr_in      sin;
unsigned char buf[BUFSIZ];
int	fd, s;
size_t	len;

        sin.sin_addr.s_addr = INADDR_ANY;
        sin.sin_port = htons((short)ip_port);
        sin.sin_family = AF_INET;

        if ((s = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
                perror ("socket");
                exit (1);
        }
        if (bind (s, (struct sockaddr *) &sin, sizeof (sin)) < 0) {
                perror("bind");
                exit (1);
        }
        listen (s, 5);
        if ((fd = accept (s, (struct sockaddr *) &sin, &len)) < 0) {
        	perror("accept");
        	exit(1);
        }
	tds = tds_alloc_socket(BUFSIZ);
	tds->s = fd;
	tds->out_flag=0x02;
	/* get_incoming(tds->s); */
	return tds;
}
int tds_read_login(TDSSOCKET *tds, TDSLOGIN *login)
{
int len,i;
char blockstr[7];
/*
	while (len = tds_read_packet(tds)) {
		for (i=0;i<len;i++)
			printf("%d %d %c\n",i, tds->in_buf[i], (tds->in_buf[i]>=' ' && tds->in_buf[i]<='z') ? tds->in_buf[i] : ' ');
	}	
*/
	tds_read_string(tds, login->host_name, 30);
	tds_read_string(tds, login->user_name, 30);
	tds_read_string(tds, login->password, 30);
	tds_read_string(tds, NULL, 30); /* host process, junk for now */
	tds_read_string(tds, NULL, 15); /* magic */
	tds_read_string(tds, login->app_name, 30); 
	tds_read_string(tds, login->server_name, 30); 
	tds_read_string(tds, NULL, 255); /* secondary passwd...encryption? */
	login->major_version = tds_get_byte(tds);
	login->minor_version = tds_get_byte(tds);
	tds_get_smallint(tds); /* unused part of protocol field */
	tds_read_string(tds, login->library, 10); 
	tds_get_byte(tds); /* program version, junk it */
	tds_get_byte(tds);
	tds_get_smallint(tds); 
	tds_get_n(tds, NULL, 3); /* magic */
	tds_read_string(tds, login->language, 30); 
	tds_get_n(tds, NULL, 14); /* magic */
	tds_read_string(tds, login->char_set, 30); 
	tds_get_n(tds, NULL, 1); /* magic */
	tds_read_string(tds, blockstr, 6); 
	printf("block size %s\n",blockstr);
	login->block_size=atoi(blockstr);
	tds_get_n(tds, NULL, tds->in_len - tds->in_pos); /* read junk at end */
}
int tds_read_string(TDSSOCKET *tds, char *dest, int size)
{
char *tempbuf;
int len;

	tempbuf = (char *) malloc(size+1);
	tds_get_n(tds,tempbuf,size);
	len=tds_get_byte(tds);
	if (dest) {
		memcpy(dest,tempbuf,len);
		dest[len]='\0';
	}
	free(tempbuf);

	return len;
}
