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

#include <config.h>
#include "pool.h"

int pool_packet_read(TDS_POOL_MEMBER *pmbr);

/*
** pool_mbr_login open a single pool login, to be call at init time or
** to reconnect.
*/
TDSSOCKET *pool_mbr_login(TDS_POOL *pool)
{
TDSLOGIN *login;
TDSSOCKET *tds;
int rc, marker;
char *query;

	login = tds_alloc_login();
	tds_set_passwd(login,pool->password);
	tds_set_user(login,pool->user);
	tds_set_app(login,"myhost");
	tds_set_host(login,"myhost");
	tds_set_library(login,"TDS-Library");
	tds_set_server(login,pool->server);
	tds_set_charset(login,"iso_1");
	tds_set_language(login,"us_english");
	tds_set_packet(login,512);
	tds = tds_alloc_socket(NULL, 512);
	tds_set_parent(tds, NULL);
  	if (tds_connect(tds, login) == TDS_FAIL) {
		/* what to do? */
		fprintf(stderr, "Could not open connection to server %s\n",pool->server);
		return NULL;
	}
	/* FIX ME -- tds_connect no longer preallocates the in_buf need to 
	** do something like what tds_read_packet does */
	tds->in_buf = (unsigned char *)malloc(BLOCKSIZ);
	memset(tds->in_buf,0,BLOCKSIZ);

	if (pool->database && strlen(pool->database)) {
		query = (char *) malloc(strlen(pool->database) + 5);
		sprintf(query, "use %s", pool->database);
		rc = tds_submit_query(tds,query);
		free(query);
		if (rc != TDS_SUCCEED) {
			fprintf(stderr, "changing database failed\n");
			return NULL;
		}

		do {
			marker=tds_get_byte(tds);
			tds_process_default_tokens(tds,marker);
		} while (marker!=TDS_DONE_TOKEN);
	}


	return tds;
}
void pool_free_member(TDS_POOL_MEMBER *pmbr)
{
	if (pmbr->tds) {
		if (pmbr->tds->s) close(pmbr->tds->s);
		pmbr->tds->s = 0;
	}
	pmbr->tds = NULL;
	/* if he is allocated disconnect the client 
	** otherwise we end up with broken client.
	*/
	if (pmbr->current_user) pool_free_user(pmbr->current_user);
	pmbr->state = TDS_COMPLETED;
}
int pool_mbr_init(TDS_POOL *pool)
{
TDS_POOL_MEMBER *pmbr;
int i;

	/* allocate room for pool members */

	pool->members = (TDS_POOL_MEMBER *) 
		malloc(sizeof(TDS_POOL_MEMBER)*pool->num_members);
	memset(pool->members,'\0', 
		sizeof(TDS_POOL_MEMBER) * pool->num_members);

	/* open connections for each member */

	for (i=0;i<pool->num_members;i++) {
		pmbr = &pool->members[i];
		if (i < pool->min_open_conn) {
			pmbr->tds = pool_mbr_login(pool);
			pmbr->last_used_tm = time(NULL);
			if (!pmbr->tds) {
				fprintf(stderr,"Could not open initial connection %d\n",i);
				exit(1);
			}
		}
		pmbr->state = TDS_COMPLETED;
	}
}

/* 
** pool_process_members
** check the fd_set for members returning data to the client, lookup the 
** client holding this member and forward the results.
*/
int pool_process_members(TDS_POOL *pool, fd_set *fds)
{
TDS_POOL_MEMBER *pmbr;
TDS_POOL_USER *puser;
TDSSOCKET *tds;
int i, len, age;
int cnt = 0;
unsigned char *buf;
time_t time_now;

	for (i=0; i < pool->num_members; i++) {
		pmbr = (TDS_POOL_MEMBER *) &pool->members[i];

		if (! pmbr->tds) break; /* dead connection */

		tds = pmbr->tds;
		time_now = time(NULL);
		if (FD_ISSET(tds->s, fds)) {
			pmbr->last_used_tm = time_now;
			cnt++;
			/* tds->in_len = read(tds->s, tds->in_buf, BLOCKSIZ); */
			if (pool_packet_read(pmbr)) continue;

			if (tds->in_len==0) {
				fprintf(stderr,"Uh oh! member %d disconnected\n",i);
				/* mark as dead */
				pool_free_member(pmbr);
			} else if (tds->in_len==-1) {
				perror("read");
			} else {
				fprintf(stderr,"read %d bytes from member %d\n",tds->in_len,i);
				if (pmbr->current_user) {
					puser = pmbr->current_user;
					buf = tds->in_buf;
					if (pool_find_end_token(pmbr,buf+8,tds->in_len-8)) {
						/* we are done...deallocate member */
						pmbr->current_user = NULL;
						pmbr->state = TDS_COMPLETED;
						puser->user_state = TDS_SRV_IDLE;
					}
					write(puser->tds->s,buf,tds->in_len);
				}
			}
		}
		age = time_now - pmbr->last_used_tm;
		if (age > pool->max_member_age && i >= pool->min_open_conn) {
			fprintf(stderr,"member %d is %d seconds old...closing\n",i,age);
			pool_free_member(pmbr);
		}
	}
	return cnt;
}

/*
** pool_find_idle_member
** returns the first pool member in TDS_COMPLETED (idle) state
*/
TDS_POOL_MEMBER *pool_find_idle_member(TDS_POOL *pool)
{
int i, active_members;
TDS_POOL_MEMBER *pmbr;

	active_members = 0;
	for (i=0;i<pool->num_members;i++) {
		pmbr = &pool->members[i];
		if (pmbr->tds)  {
			active_members++;
			if (pmbr->state==TDS_COMPLETED) {
				/* make sure member wasn't idle more that the timeout 
				** otherwise it'll send the query and close leaving a
				** hung client */
				pmbr->last_used_tm = time(NULL);
				return pmbr;
			}
		}
	}
	/* if we have dead connections we can open */
	if (active_members < pool->num_members) {
		for (i=0;i<pool->num_members;i++) {
			pmbr = &pool->members[i];
			if (!pmbr->tds) {
				fprintf(stderr,"No open connections left, opening member number %d\n",i);
				pmbr->tds = pool_mbr_login(pool);
				pmbr->last_used_tm = time(NULL);
				break;
			}
		}
		if (pmbr) return pmbr;
	}
	fprintf(stderr,"No idle members left, increase MAX_POOL_CONN\n");
	return NULL;
}
int pool_packet_read(TDS_POOL_MEMBER *pmbr)
{
TDSSOCKET *tds;
int packet_len;

	tds = pmbr->tds;
	
	if (pmbr->need_more) {
		tds->in_len += read(tds->s, &tds->in_buf[tds->in_len], BLOCKSIZ - tds->in_len);
	} else {
		tds->in_len = read(tds->s, tds->in_buf, BLOCKSIZ);
	}	
	packet_len = ntohs(*(short*)&tds->in_buf[2]);
	if (tds->in_len < packet_len) {
		pmbr->need_more = 1;
	} else {
		pmbr->need_more = 0;
	}
	return pmbr->need_more;
}
