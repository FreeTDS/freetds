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
#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef WIN32
#define READ(a,b,c) recv (a, b, c, 0L);
#else
#define READ(a,b,c) read (a, b, c);
#endif

#include "tdsutil.h"


static char  software_version[]   = "$Id: read.c,v 1.19 2002-09-22 08:01:47 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


/** 
 * Loops until we have received buflen characters 
 * return -1 on failure 
 */
static int
goodread (TDSSOCKET *tds, unsigned char *buf, int buflen)
{
int got = 0;
int len, retcode;
fd_set fds;
time_t start, now;
struct timeval selecttimeout;

	FD_ZERO (&fds);
	if (tds->timeout) {
		start = time(NULL);
		now = start;

		/* FIXME return even if not finished read if timeout */
		while ((buflen > 0) && ((now-start) < tds->timeout)) {
			int timeleft = tds->timeout;
			len = 0;
			retcode = 0;

			do {
				FD_SET (tds->s, &fds);
				selecttimeout.tv_sec = timeleft;
				selecttimeout.tv_usec = 0;
				retcode = select (tds->s + 1, &fds, NULL, NULL, &selecttimeout);
				/* ignore EINTR errors */
				if (retcode < 0 && errno == EINTR)
					retcode = 0;

				now = time (NULL);
				timeleft = tds->timeout - (now-start);
			} while((retcode == 0) && timeleft > 0);
			len = READ(tds->s, buf+got, buflen);
			if (len <= 0) {
				if (len < 0 && errno == EINTR) len = 0;
				else return (-1); /* SOCKET_ERROR); */
			}

			buflen -= len;
			got += len;
			now = time (NULL);

			if (tds->queryStarttime && tds->longquery_timeout) {
				if ((now-(tds->queryStarttime)) >= tds->longquery_timeout) {
					(*tds->longquery_func)(tds->longquery_param);
				}
			}
		} /* while buflen... */
	} else {
		/* got = READ(tds->s, buf, buflen); */
		while (got < buflen) {
			int len;
			FD_SET (tds->s, &fds);
			select (tds->s + 1, &fds, NULL, NULL, NULL);
			len = READ(tds->s, buf + got, buflen - got);
			if (len <= 0) {
				if (len < 0 && (errno == EINTR || errno == EINPROGRESS)) len = 0;
				else return (-1); /* SOCKET_ERROR); */
			}  
			got += len;
		}

	}

	return (got);
}

/*
** Return a single byte from the input buffer
*/
unsigned char
tds_get_byte(TDSSOCKET *tds)
{
int rc;

	if (tds->in_pos >= tds->in_len) {
		while (!IS_TDSDEAD(tds) && (rc = tds_read_packet(tds)) == 0)
			;
		if (IS_TDSDEAD(tds) || rc == -1) {
			return 0;
		}
	}
	return tds->in_buf[tds->in_pos++];
}
/*
** Unget will always work as long as you don't call it twice in a row.  It
** it may work if you call it multiple times as long as you don't backup
** over the beginning of network packet boundary which can occur anywhere in
** the token stream.
*/
void tds_unget_byte(TDSSOCKET *tds)
{
	/* this is a one trick pony...don't call it twice */
	tds->in_pos--;
}

unsigned char tds_peek(TDSSOCKET *tds)
{
   unsigned char   result = tds_get_byte(tds);
   tds_unget_byte(tds);
   return result;
} /* tds_peek()  */


/*
** Get an int16 from the server.
*/
TDS_SMALLINT tds_get_smallint(TDSSOCKET *tds)
{
unsigned char bytes[2];

	tds_get_n(tds, bytes, 2);
#if WORDS_BIGENDIAN
	if (tds->emul_little_endian) {
		return TDS_BYTE_SWAP16(*(TDS_SMALLINT *)bytes);
	}
#endif
	return *(TDS_SMALLINT *)bytes;
}


/*
** Get an int32 from the server.
*/
TDS_INT tds_get_int(TDSSOCKET *tds)
{
unsigned char bytes[4];

	tds_get_n(tds, bytes, 4);
#if WORDS_BIGENDIAN
	if (tds->emul_little_endian) {
		return TDS_BYTE_SWAP32(*(TDS_INT *)bytes);
	}
#endif
	return *(TDS_INT *)bytes;
}


/*
 * fetches a null terminated ascii string
 */
char *tds_get_ntstring(TDSSOCKET *tds, char *dest, int max)
{
int i = 0;
char c;

	while ((c = tds_get_byte(tds))) {
		if (i < (max - 1) && dest)
			dest[i++] = c;
	}
	if (dest) dest[i]='\0';

	return dest;
}
char *tds_get_string(TDSSOCKET *tds, void *dest, int need)
{
char *temp;

	/*
	** FIX: 02-Jun-2000 by Scott C. Gray (SCG)
	**
	** Bug to malloc(0) on some platforms.
	*/
	if (need == 0) {
		return dest;
	}

	if (IS_TDS70(tds) || IS_TDS80(tds)) {
		if (dest==NULL) {
			tds_get_n(tds,NULL,need*2);
			return(NULL);
		}
		/* FIXME handle allocation error, use chunk conversions */
		temp = (char *) malloc(need*2);
		tds_get_n(tds,temp,need*2);
		tds7_unicode2ascii(tds,temp,dest,need);
		free(temp);
		return(dest);
		
	} else {
		return tds_get_n(tds,dest,need);	
	}
}
/*
** Get N bytes from the buffer and return them in the already allocated space  
** given to us.  We ASSUME that the person calling this function has done the  
** bounds checking for us since they know how many bytes they want here.
** dest of NULL means we just want to eat the bytes.   (tetherow@nol.org)
*/
char *tds_get_n(TDSSOCKET *tds, void *dest, int need)
{
int pos,have;

	pos = 0;

	have=(tds->in_len-tds->in_pos);
	while (need>have) {
		/* We need more than is in the buffer, copy what is there */
		if (dest!=NULL) {
			memcpy((char*)dest+pos, tds->in_buf+tds->in_pos, have);
		}
		pos+=have;
		need-=have;
		tds_read_packet(tds);
		have=tds->in_len;
	}
	if (need>0) {
		/* get the remainder if there is any */
		if (dest!=NULL) {
			memcpy((char*)dest+pos, tds->in_buf+tds->in_pos, need);
		}
		tds->in_pos+=need;
	}
	return dest;
}
/*
** Return the number of bytes needed by specified type.
*/
int get_size_by_type(int servertype)
{
   switch(servertype)
   {
      case SYBINT1:        return 1;  break;
      case SYBINT2:        return 2;  break;
      case SYBINT4:        return 4;  break;
      case SYBINT8:        return 8;  break;
      case SYBREAL:        return 4;  break;
      case SYBFLT8:        return 8;  break;
      case SYBDATETIME:    return 8;  break;
      case SYBDATETIME4:   return 4;  break;
      case SYBBIT:         return 1;  break;
      case SYBBITN:        return 1;  break;
      case SYBMONEY:       return 8;  break;
      case SYBMONEY4:      return 4;  break;
	 case SYBUNIQUE:      return 16; break;
      default:             return -1; break;
   }
}
/*
** Read in one 'packet' from the server.  This is a wrapped outer packet of
** the protocol (they bundle resulte packets into chunks and wrap them at
** what appears to be 512 bytes regardless of how that breaks internal packet
** up.   (tetherow@nol.org)
*/
int tds_read_packet (TDSSOCKET *tds)
{
unsigned char header[8];
int           len;
int           x = 0, have, need;


	/* Read in the packet header.  We use this to figure out our packet 
	** length */

	/* Cast to int are needed because some compiler seem to convert
	 * len to unsigned (as FreeBSD 4.5 one)*/
	if ((len = goodread(tds, header, sizeof(header))) < (int)sizeof(header) ) {
		/* GW ADDED */
		if (len<0) {
			/* FIX ME -- get the exact err num and text */
			tds_client_msg(tds->tds_ctx, tds,10018, 9, 0, 0, "The connection was closed");
			tds_close_socket(tds);
                	tds->in_len=0;
			tds->in_pos=0;
			return -1;
		}
		/* GW ADDED */
		/*  Not sure if this is the best way to do the error 
		**  handling here but this is the way it is currently 
		**  being done. */
                tds->in_len=0;
		tds->in_pos=0;
		tds->last_packet=1;
		if (len==0) {
			tds_close_socket(tds);
		}
		return -1;
	}
	tdsdump_log(TDS_DBG_NETWORK, "Received header @ %L\n%D\n", header, sizeof(header));

/* Note:
** this was done by Gregg, I don't think its the real solution (it breaks
** under 5.0, but I haven't gotten a result big enough to test this yet.
*/
	if (IS_TDS42(tds)) {
		if (header[0]!=0x04 && header[0]!=0x0f) {
			tdsdump_log(TDS_DBG_ERROR, "Invalid packet header %d\n", header[0]);
			/*  Not sure if this is the best way to do the error 
			**  handling here but this is the way it is currently 
			**  being done. */
			tds->in_len=0;
			tds->in_pos=0;
			tds->last_packet=1;
			return(-1);
		}
	}
 
        /* Convert our packet length from network to host byte order */
        len = ((((unsigned int)header[2])<<8)|header[3])-8;
        need=len;

        /* If this packet size is the largest we have gotten allocate 
	** space for it */
	if (len > tds->in_buf_max) {
		unsigned char *p;
		if (! tds->in_buf) {
			p = (unsigned char*)malloc(len);
		} else {
			p = (unsigned char*)realloc(tds->in_buf, len);
		}
		if (!p) return -1; /* FIXME should close socket too */
		tds->in_buf = p;
		/* Set the new maximum packet size */
		tds->in_buf_max = len;
	}

	/* Clean out the in_buf so we don't use old stuff by mistake */
	memset(tds->in_buf, 0, tds->in_buf_max);

	/* Now get exactly how many bytes the server told us to get */
	have=0;
	while(need>0) {
		if ((x=goodread(tds, tds->in_buf+have, need))<1) {
			/*  Not sure if this is the best way to do the error 
			**  handling here but this is the way it is currently 
			**  being done. */
			tds->in_len=0;
			tds->in_pos=0;
			tds->last_packet=1;
			if (len==0) {
				tds_close_socket(tds);
			}
			return(-1);
		}
		have+=x;
		need-=x;
	}
	if (x < 1 ) {
		/*  Not sure if this is the best way to do the error handling 
		**  here but this is the way it is currently being done. */
		tds->in_len=0;
		tds->in_pos=0;
		tds->last_packet=1;
		/* return 0 if header found but no payload */
		return len ? -1 : 0;
	}

	/* Set the last packet flag */
	if (header[1]) { tds->last_packet = 1; }
	else           { tds->last_packet = 0; }

	/* Set the length and pos (not sure what pos is used for now */
	tds->in_len = have;
	tds->in_pos = 0;
	tdsdump_log(TDS_DBG_NETWORK, "Received packet @ %L\n%D\n", tds->in_buf, tds->in_len);

	return (tds->in_len);
}
