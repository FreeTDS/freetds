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
#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef WIN32
#define WRITE(a,b,c) send((a),(b),(c), 0L)
#else 
#define WRITE(a,b,c) write(a,b,c)
#endif

static char  software_version[]   = "$Id: write.c,v 1.16 2002-09-26 11:26:36 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


/*
 * CRE 01262002 making buf a void * means we can put any type without casting
 *		much like read() and memcpy()
 */
int
tds_put_n(TDSSOCKET *tds, const unsigned char *buf, int n)
{
int i;
const unsigned char *bufp = buf;
	if (bufp)  {
		for (i=0;i<n;i++)	
			tds_put_byte(tds, bufp[i]);
	} else {
		for (i=0;i<n;i++)	
			tds_put_byte(tds, '\0');
	}
	return 0;
}

/**
 * Output a string to wire
 * automatic translate string to unicode if needed
 */
int tds_put_string(TDSSOCKET *tds, const char *s)
{
int len = strlen(s),res;
char *temp;

	if (IS_TDS7_PLUS(tds)) {
		/* FIXME handle allocation error, use chunk conversions */
		temp = (char *) malloc(len*2);
		tds7_ascii2unicode(tds, s, temp, len*2);
		res = tds_put_n(tds, temp, len*2);
		free(temp);
		return res;
		
	}
	return tds_put_n(tds,s,len);
}

int tds_put_padded_cstring(TDSSOCKET *tds, const char *buf, int n)
{
int buf_len = ( buf ? strlen(buf) : 0);

	return tds_put_buf(tds,(const unsigned char *)buf,n,buf_len);
}
int tds_put_buf(TDSSOCKET *tds, const unsigned char *buf, int dsize, int ssize)
{
int  cpsize;

	cpsize = ssize > dsize ? dsize : ssize;
	tds_put_n(tds,buf,cpsize);
	dsize -= cpsize;
	tds_put_n(tds,NULL,dsize);
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
	return tds_put_n(tds,(const unsigned char *)&i,sizeof(TDS_INT));
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
	return tds_put_n(tds,(const unsigned char *)&si,sizeof(TDS_SMALLINT));
}
int tds_put_tinyint(TDSSOCKET *tds, TDS_TINYINT ti)
{
	return tds_put_byte(tds,(unsigned char)ti);
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
int tds_put_bulk_data(TDSSOCKET *tds, const unsigned char * buf, TDS_INT bufsize)
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

/* TODO this code should be similar to read one... */
static int 
tds_check_socket_write(TDSSOCKET *tds)
{
int retcode = 0;
struct timeval selecttimeout;
time_t start, now;
fd_set fds;
 
	/* Jeffs hack *** START OF NEW CODE */
	FD_ZERO (&fds);

	if (!tds->timeout) {
		for(;;) {
			FD_SET (tds->s, &fds);
			retcode = select (tds->s + 1, NULL, &fds, NULL, NULL);
			/* write available */
			if (retcode >= 0) return 0;
			/* interrupted */
			if (errno == EINTR) continue;
			/* error, leave caller handle problems*/
			return -1;
		}
	}
	start = time(NULL);
	now = start;
 
	while ((retcode == 0) && ((now-start) < tds->timeout)) {
		FD_SET (tds->s, &fds);
		selecttimeout.tv_sec = tds->timeout - (now-start);
		selecttimeout.tv_usec = 0;
		retcode = select (tds->s + 1, NULL, &fds, NULL, &selecttimeout);
		if (retcode < 0 && errno == EINTR) {
			retcode = 0;
		}

		now = time (NULL);
	}
 
	return retcode;
	/* Jeffs hack *** END OF NEW CODE */
}

/* goodwrite function adapted from patch by freddy77 */
static int
goodwrite(TDSSOCKET *tds)
{
int left;
char *p;
int result = 1;
int retval;

	left = tds->out_pos;
	p = tds->out_buf;
  
	while (left > 0) {
		/* If there's a timeout, we need to sit and wait for socket */
		/* writability */
		/* moved socket writability check to own function -- bsb */
		tds_check_socket_write(tds);

		retval = WRITE(tds->s,p,left);

		if (retval <= 0) {
			tdsdump_log(TDS_DBG_NETWORK, "TDS: Write failed in tds_write_packet\nError: %d (%s)\n", errno, strerror(errno));
			tds_client_msg(tds->tds_ctx, tds, 20006, 9, 0, 0, "Write to SQL Server failed.");
			tds->in_pos=0;
			tds->in_len=0;
			tds_close_socket(tds);
			result = 0;
			break;
		}
		left -= retval;
		p += retval;
	}
	return result;
}
int tds_write_packet(TDSSOCKET *tds,unsigned char final)
{
int retcode;
void (*oldsig)(int);

	tds->out_buf[0]=tds->out_flag;
	tds->out_buf[1]=final;
	tds->out_buf[2]=(tds->out_pos)/256u;
	tds->out_buf[3]=(tds->out_pos)%256u;
	if (IS_TDS70(tds) || IS_TDS80(tds)) {
		tds->out_buf[6]=0x01;
	}

	tdsdump_log(TDS_DBG_NETWORK, "Sending packet @ %L\n%D\n", tds->out_buf, tds->out_pos);

	oldsig=signal (SIGPIPE, SIG_IGN);
	if (oldsig==SIG_ERR) {
		tdsdump_log(TDS_DBG_WARN, "TDS: Warning: Couldn't set SIGPIPE signal to be ignored\n");
	}

	retcode=goodwrite(tds);

	if (signal(SIGPIPE, oldsig)==SIG_ERR) {
		tdsdump_log(TDS_DBG_WARN, "TDS: Warning: Couldn't reset SIGPIPE signal to previous value\n");
	}
/* GW added in check for write() returning <0 and SIGPIPE checking */
	return retcode;
}

int
tds_flush_packet(TDSSOCKET *tds)
{
	if (!IS_TDSDEAD(tds)) {
		tds_write_packet(tds,0x01);
		tds_init_write_buf(tds);
	}
	/* GW added check for tds->s */
	return 0;
}
