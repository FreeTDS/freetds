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
#include <signal.h> /* GW ADDED */

#ifdef WIN32
#define WRITE(a,b,c) send((a),(b),(c), 0L)
#else 
#define WRITE(a,b,c) write(a,b,c)
#endif

static char  software_version[]   = "$Id: write.c,v 1.3 2001-11-08 17:53:06 mlilback Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


int tds_put_n(TDSSOCKET *tds, unsigned char *buf, int n)
{
int i;
	if (buf)  {
		for (i=0;i<n;i++)	
			tds_put_byte(tds,buf[i]);
	} else {
		for (i=0;i<n;i++)	
			tds_put_byte(tds,'\0');
	}
	return 0;
}
int tds_put_string(TDSSOCKET *tds, unsigned char *buf, int n)
{
int buf_len = ( buf ? strlen(buf) : 0);

	return tds_put_buf(tds,buf,n,buf_len);
}
int tds_put_buf(TDSSOCKET *tds, unsigned char *buf, int dsize, int ssize)
{
char *tempbuf;
int  cpsize;

	tempbuf = (char *) malloc(dsize+1);
	memset(tempbuf,'\0',dsize);
	cpsize = ssize > dsize ? dsize : ssize;
	if (buf) memcpy(tempbuf,buf, cpsize);
	tds_put_n(tds,tempbuf,dsize);
	free(tempbuf);
	return tds_put_byte(tds,cpsize);
}
int tds_put_int(TDSSOCKET *tds, TDS_INT i)
{
#if WORDS_BIGENDIAN
	if (tds->emul_little_endian) {
		tds_put_byte(tds, i & 0x000000FF);
		tds_put_byte(tds, (i & 0x0000FF00) >> 8);
		tds_put_byte(tds, (i & 0x00FF0000) >> 16);
		tds_put_byte(tds, (i & 0xFF000000) >> 24);
		return 0;
	}
#endif
	return tds_put_n(tds,(unsigned char *)&i,sizeof(TDS_INT));
}
int tds_put_smallint(TDSSOCKET *tds, TDS_SMALLINT si)
{
#if WORDS_BIGENDIAN
	if (tds->emul_little_endian) {
		tds_put_byte(tds, si & 0x000000FF);
		tds_put_byte(tds, (si & 0x0000FF00) >> 8);
		return 0;
	}
#endif
	return tds_put_n(tds,(unsigned char *)&si,sizeof(TDS_SMALLINT));
}
int tds_put_tinyint(TDSSOCKET *tds, TDS_TINYINT ti)
{
	return tds_put_n(tds,(unsigned char *)&ti,sizeof(TDS_TINYINT));
}
int tds_put_byte(TDSSOCKET *tds, unsigned char c)
{
	if (tds->out_pos >= tds->env->block_size) {
		tds_write_packet(tds,0x0);
		tds_init_write_buf(tds);
	}
	tds->out_buf[tds->out_pos++]=c;
	return 0;
}
int tds_put_bulk_data(TDSSOCKET *tds, unsigned char * buf, TDS_INT bufsize)
{

	tds->out_flag = 0x07;
	return tds_put_n(tds,buf,bufsize);
}
int tds_init_write_buf(TDSSOCKET *tds)
{
	memset(tds->out_buf,'\0',tds->env->block_size);
	tds->out_pos=8;
	return 0;
}
int tds_write_packet(TDSSOCKET *tds,unsigned char final)
{
static int retval;
void (*oldsig)(int);
fd_set fds;
struct timeval selecttimeout;
time_t start, now;
int retcode = 0;

	tds->out_buf[0]=tds->out_flag;
	tds->out_buf[1]=final;
	tds->out_buf[2]=(tds->out_pos)/256;
	tds->out_buf[3]=(tds->out_pos)%256;
	if (IS_TDS70(tds)) {
		tds->out_buf[6]=0x01;
	}

        tdsdump_log(TDS_DBG_NETWORK, "Sending packet @ %L\n%D\n", tds->out_buf, tds->out_pos);
	oldsig=signal (SIGPIPE, SIG_IGN);
	if (oldsig==SIG_ERR) {
		fprintf(stderr, "TDS: Warning: Couldn't set SIGPIPE signal to be ignored\n");
	}

     /* Jeffs hack *** NEW CODE */
     /* If there's a timeout, we need to sit and wait for socket writability */
	if (tds->timeout) {
		start = time(NULL);

		FD_ZERO (&fds);
    
		selecttimeout.tv_sec = 0;
		selecttimeout.tv_usec = 0;

		now = time(NULL);

		while ((retcode == 0) && ((now-start) < tds->timeout)) {
			tds_msleep(1);
			FD_SET (tds->s, &fds);
			selecttimeout.tv_sec = 0;
			selecttimeout.tv_usec = 0;
			retcode = select (tds->s + 1, NULL, &fds, NULL, &selecttimeout);
			if (retcode < 0 && errno == EINTR) {
				retcode = 0;
			}

			now = time (NULL);
          }
	}
/* Jeffs hack *** END OF NEW CODE */
	retval=write(tds->s,tds->out_buf,tds->out_pos);

	if (signal(SIGPIPE, oldsig)==SIG_ERR) {
 		fprintf(stderr, "TDS: Warning: Couldn't reset SIGPIPE signal to previous value\n");
	}
 	if (retval < 0) {
		fprintf(stderr, "TDS: Write failed in tds_write_packet\nError: %d (%s)\n", errno, strerror(errno));
		tds_client_msg(tds, 10018, 9, 0, 0, "The connection was closed");
		tds->in_pos=0;
		tds->in_len=0;
		close(tds->s);
		tds->s=0;
		return 0;
	}
/* GW added in check for write() returning <0 and SIGPIPE checking */
	return 1;
}
int tds_flush_packet(TDSSOCKET *tds)
{
	if (tds->s) {
		tds_write_packet(tds,0x01);
		tds_init_write_buf(tds);
	}
	/* GW added check for tds->s */
	return 0;
}
