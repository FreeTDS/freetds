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

#ifdef WIN32
#define CLOSE(a) closesocket(a)
#define READ(a,b,c) recv (a, b, c, 0L);
#else
#define CLOSE(a) close(a)
#define READ(a,b,c) read (a, b, c);
#endif

#include "tdsutil.h"


static char  software_version[]   = "$Id: read.c,v 1.6 2002-03-27 22:04:46 vorlon Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


/* Loops until we have received buflen characters */
static int
goodread (TDSSOCKET *tds, unsigned char *buf, int buflen)
{
int got = 0;
int len, retcode;
fd_set fds;
time_t start, now;
struct timeval selecttimeout;

	if (tds->timeout) {
		start = time(NULL);
		now = time(NULL);

		while ((buflen > 0) && ((now-start) < tds->timeout)) {
			len = 0;
			retcode = 0;

			FD_ZERO (&fds);
			selecttimeout.tv_sec = 0;
			selecttimeout.tv_usec = 0;

			now = time(NULL);

			FD_SET (tds->s, &fds);
			retcode = select (tds->s + 1, &fds, NULL, NULL, &selecttimeout);

			while ((retcode == 0) && ((now-start) < tds->timeout)) {
				tds_msleep(1);

				FD_SET (tds->s, &fds);
				selecttimeout.tv_sec = 0;
				selecttimeout.tv_usec = 0;
				retcode = select (tds->s + 1, &fds, NULL, NULL, &selecttimeout);

				now = time (NULL);
			}
			len = READ(tds->s, buf+got, buflen);

			/* FIXME: we should do proper handling of EINTR here as well. */
			if (len <= 0) {
				return (-1); /* SOCKET_ERROR); */
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
			int len = READ(tds->s, buf + got, buflen - got);
			if (len <= 0 && errno != EINTR) {
				return (-1); /* SOCKET_ERROR); */
			}  
			got += len;
		}

	}

	return (got);
}

/*
** Return a single byte from the input buffer
*/
unsigned char tds_get_byte(TDSSOCKET *tds)
{
	if (tds->in_pos >= tds->in_len) {
		if(!tds->s || !tds_read_packet(tds))
			return (0);
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

	if ((len = goodread(tds, header, sizeof(header))) < sizeof(header) ) {
		/* GW ADDED */
		if (len<0) {
			/* FIX ME -- get the exact err num and text */
			tds_client_msg(tds,10018, 9, 0, 0, "The connection was closed");
			CLOSE(tds->s);
			tds->s=0;
                	tds->in_len=0;
			tds->in_pos=0;
			return 0;
		}
		/* GW ADDED */
		/*  Not sure if this is the best way to do the error 
		**  handling here but this is the way it is currently 
		**  being done. */
                tds->in_len=0;
		tds->in_pos=0;
		tds->last_packet=1;
		if (len==0) {
			CLOSE(tds->s);
			tds->s=0;
		}
		return 0;
	}

/* Note:
** this was done by Gregg, I don't think its the real solution (it breaks
** under 5.0, but I haven't gotten a result big enough to test this yet.
*/
	if (IS_TDS42(tds)) {
		if (header[0]!=0x04) {
			fprintf(stderr, "Invalid packet header %d\n", header[0]);
			/*  Not sure if this is the best way to do the error 
			**  handling here but this is the way it is currently 
			**  being done. */
			tds->in_len=0;
			tds->in_pos=0;
			tds->last_packet=1;
			return(0);
		}
	}
 
        /* Convert our packet length from network to host byte order */
        /* Only safe as long as local short length is 2 bytes */
        len = ntohs(*(short*)&header[2])-8;
        need=len;

        /* If this packet size is the largest we have gotten allocate 
	** space for it */
	if (len > tds->in_buf_max) {
		if (! tds->in_buf) {
			tds->in_buf = (unsigned char*)malloc(len);
		} else {
			tds->in_buf = (unsigned char*)realloc(tds->in_buf, len);
		}
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
				CLOSE(tds->s);
				tds->s=0;
			}
			return(0);
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
		return(0);
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
