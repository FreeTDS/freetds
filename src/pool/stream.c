/* TDSPool - Connection pooling for TDS based databases
 * Copyright (C) 2001 Brian Bruns
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
** Name: stream.c
** Description: Controls the result stream processing.
*/

#include <config.h>
#include "pool.h"
#include "tds.h"

struct tmp_col_struct {
        char *column_name;
        struct tmp_col_struct *next;
};

/*
**  returns 1 if this end token has the final bit set
*/
static int is_final_token(const unsigned char *buf) 
{
	return buf[1] & 0x01 ? 0 : 1;
}

/*
**  computes the number of bytes left to be read from the input stream 
**  after this packet is completely processed.
**  returns 1 if token overruns the current packet, 0 otherwise.
*/
static int bytes_left(TDS_POOL_MEMBER *pmbr,
	const unsigned char *buf,
	int pos,
	int maxlen,
	int need)
{
	if (pos+need > maxlen) {
		return 1;
	} else
		return 0;
}
/*
** attempts to read a fixed length token, returns 1 if successful
** bytes_read is set to the number of bytes read fromt the input stream.
*/
static int read_fixed_token(TDS_POOL_MEMBER *pmbr,
	const unsigned char *buf,
	int maxlen,
	int *bytes_read) 
{
TDS_SMALLINT sz;
int marker;
int pos = 0;

	if (bytes_left(pmbr, buf, pos, maxlen, 1)) {
		*bytes_read = maxlen;
		return 0;
	}
	marker = buf[pos++];
	sz = tds_get_token_size(marker);
	if (bytes_left(pmbr, buf, pos, maxlen, sz)) {
		*bytes_read = maxlen;
		return 0;
	} else {
		*bytes_read = sz + 1;
		return 1;
	}
}
static int read_variable_token(TDS_POOL_MEMBER *pmbr,
	const unsigned char *buf,
	int maxlen,
	int *bytes_read) 
{
TDS_SMALLINT sz;
int pos = 0;

	if (bytes_left(pmbr, buf, pos, maxlen, 3)) {
		*bytes_read = maxlen;
		return 0;
	}

	/* memcpy(&sz,&buf[1],2); */
	/* FIX ME -- works only in emul little endian mode on bigend boxen */
	sz = buf[1] + buf[2] * 256;
	pos+=3;
	if (bytes_left(pmbr, buf, pos, maxlen, sz)) {
		*bytes_read = maxlen;
		return 0;
	} else {
		*bytes_read = sz + 3;
		return 1;
	}
}
static int read_row(TDS_POOL_MEMBER *pmbr,
	const unsigned char *buf,
	int maxlen,
	int *bytes_read) 
{
TDSCOLINFO *curcol;
TDSRESULTINFO *info;
int i, colsize;
int pos = 1; /* skip marker */

	info = pmbr->tds->res_info;

	die_if((!info), "Entered read_row() without a res_info structure.");

	for (i=0;i<info->num_cols;i++) {
		curcol = info->columns[i];
		if (!is_fixed_type(curcol->column_type)) {
			if (bytes_left(pmbr, buf, pos, maxlen, 1)) {
				*bytes_read = maxlen;
				return 0;
			}
			colsize = buf[pos++];
		} else {
			colsize = get_size_by_type(curcol->column_type);
                }
		if (bytes_left(pmbr, buf, pos, maxlen, colsize)) {
			*bytes_read = maxlen;
			return 0;
		}
		pos += colsize;
	}

	*bytes_read = pos;

	return 1;
}
static int read_col_name(TDS_POOL_MEMBER *pmbr,
	const unsigned char *buf,
	int maxlen,
	int *bytes_read) 
{
TDS_SMALLINT hdrsize;
int pos = 0;
int stop = 0, num_cols = 0; 
int namelen;
struct tmp_col_struct *head=NULL, *cur=NULL, *prev;
int col;
TDSCOLINFO *curcol;
TDSRESULTINFO *info;

	/* header size */
	if (bytes_left(pmbr, buf, pos, maxlen, 3)) {
		*bytes_read = maxlen;
		return 0;
	}
	/* FIX ME -- endian */
	hdrsize = buf[1] + buf[2]*256;
	pos += 3;

	while (!stop) {
                prev = cur;
                cur = (struct tmp_col_struct *)
                        malloc(sizeof (struct tmp_col_struct));
                if (prev) prev->next=cur;
                if (!head) head = cur;

		if (bytes_left(pmbr, buf, pos, maxlen, 1)) {
			*bytes_read = maxlen;
			return 0;
		}	
		namelen = buf[pos++];

	
		if (bytes_left(pmbr, buf, pos, maxlen, namelen)) {
			*bytes_read = maxlen;
			return 0;
		}	
		cur->column_name = (char *) malloc(namelen+1);
		strncpy(cur->column_name, &buf[pos], namelen);
		cur->column_name[namelen]='\0';
		cur->next=NULL;

		pos += namelen;

		num_cols++;

		if (pos >= hdrsize) stop=1;
	}

	tds_free_all_results(pmbr->tds);
        pmbr->tds->res_info = tds_alloc_results(num_cols);
	info = pmbr->tds->res_info;

	cur=head;

	for (col=0;col<info->num_cols;col++) {
		curcol=info->columns[col];
		strncpy(curcol->column_name, cur->column_name,
		sizeof(curcol->column_name));
		prev=cur; cur=cur->next;
		free(prev->column_name);
		free(prev);
	}

	
	*bytes_read = pos;
	return 1;
}
static int read_col_info(TDS_POOL_MEMBER *pmbr,
	const unsigned char *buf,
	int maxlen,
	int *bytes_read) 
{
TDS_SMALLINT hdrsize;
int pos = 0;
int col, rest;
TDSCOLINFO *curcol;
TDSRESULTINFO *info;
TDSSOCKET *tds = pmbr->tds;
	
	/* header size */
	if (bytes_left(pmbr, buf, pos, maxlen, 3)) {
		*bytes_read = maxlen;
		return 0;
	}
	/* FIX ME -- endian */
	hdrsize = buf[1] + buf[2]*256;
	pos += 3;

	info = tds->res_info;
	for (col=0; col<info->num_cols; col++) {
		curcol=info->columns[col];
		if (bytes_left(pmbr, buf, pos, maxlen, 5)) {
			*bytes_read = maxlen;
			return 0;
		}
		pos +=4;
		curcol->column_type = buf[pos++];

		/* FIX ME -- blob types */
		if (!is_fixed_type(curcol->column_type)) {
			if (bytes_left(pmbr, buf, pos, maxlen, 1)) {
				*bytes_read = maxlen;
				return 0;
			}
			curcol->column_size = buf[pos++];
		} else {
			curcol->column_size = get_size_by_type(curcol->column_type);
		}
	}

	rest = hdrsize + 3 - pos;
        if (rest > 0) {
		if (bytes_left(pmbr, buf, pos, maxlen, rest)) {
			*bytes_read = maxlen;
			return 0;
		}
                fprintf(stderr,"read_col_info: draining %d bytes\n", rest);
                pos += rest;
        }
	*bytes_read = pos;

	return 1;
}
/* 
** is_end_token processes one token and returns 0 for an incomplete token
** or -1 if this is the END token, and 1 for successful processing of token.  
** The number of bytes read from the stream is returned in 'bytes_read'.
*/
static int pool_is_end_token(TDS_POOL_MEMBER *pmbr,
	const unsigned char *buf,
	int maxlen,
	int *bytes_read) 
{
TDS_SMALLINT sz;
int marker;
int ret;

	if (maxlen == 0) return 0;

	marker = buf[0];
	sz = tds_get_token_size(marker);

	if (sz) {
		ret = read_fixed_token(pmbr, buf, maxlen, bytes_read);
	} else if (marker == TDS_ROW_TOKEN) {
		ret = read_row(pmbr, buf, maxlen, bytes_read);
	} else if (marker == TDS_COL_NAME_TOKEN) {
		ret = read_col_name(pmbr, buf, maxlen, bytes_read);
	} else if (marker == TDS_COL_INFO_TOKEN) {
		ret = read_col_info(pmbr, buf, maxlen, bytes_read);
	} else {
		ret = read_variable_token(pmbr, buf, maxlen, bytes_read);
	}

	/* fprintf(stderr,"bytes_read = %d\n",*bytes_read); */
	if (!ret) {
		return 0;
	}

	if (is_end_token(marker)) {
		if (is_final_token(buf)) {
			/* clean up */
			return -1;
		}
	}

	return 1;
}
int pool_find_end_token(TDS_POOL_MEMBER *pmbr,const unsigned char *buf,int len) 
{
int pos = 0, startpos, ret;
int bytes_read;
int stop = 0;
char *curbuf;
char tmpbuf[PGSIZ+BLOCKSIZ];
int  totlen;
static int last_mark;

	/*
	** if we had part of a token left over, copy to tmpbuf
	*/
	if (pmbr->num_bytes_left) {
		/* fprintf(stderr,"%d bytes left from last packet\n",pmbr->num_bytes_left); */
		if (pmbr->num_bytes_left > 2000) {
			fprintf(stderr,"marker is %d last mark was %d\n", pmbr->fragment[0], last_mark);
		}
		memcpy(tmpbuf,pmbr->fragment,pmbr->num_bytes_left);
		memcpy(&tmpbuf[pmbr->num_bytes_left],buf,len);
		curbuf = tmpbuf;
		totlen = len + pmbr->num_bytes_left;
	} else {
		curbuf = (unsigned char *) buf;
		totlen = len;
	}

	do {
		startpos = pos;

		ret = pool_is_end_token(pmbr, &curbuf[pos], totlen-pos, &bytes_read);
		if (ret<=0) stop = 1;

		pos += bytes_read;
	} while(!stop);

	if (ret == -1) {
		pmbr->num_bytes_left = 0;
		return 1; /* found end token */
	} else {
		last_mark = buf[0];
		pmbr->num_bytes_left = totlen - startpos;
		memcpy(pmbr->fragment,&curbuf[startpos], pmbr->num_bytes_left);
		return 0; /* exhausted the stream */
	}
}
