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
** Note on terminology: a pool member is a connection to the database,
** a pool user is a client connection that is temporarily assigned to a
** pool member.
*/

#include <config.h>
#include "pool.h"
#include <signal.h>

/* this will go away...starting with just 1 global pool */
TDS_POOL *g_pool = NULL;
/* to be set by sig term */
int term = 0;
/* number of users in wait state */
int waiters = 0;
int hack = 0;

void term_handler(int sig)
{
	fprintf(stdout,"Shutdown Requested\n");
	term = 1;
}

/*
** pool_init creates a named pool and opens connections to the database
*/
TDS_POOL *pool_init(char *name)
{
int failed = 0;
TDS_POOL *pool;
TDS_POOL_MEMBER *pmbr;
int i;

	/* initialize the pool */

	g_pool = (TDS_POOL *) malloc(sizeof(TDS_POOL));	
	pool = g_pool;
	memset(pool,'\0',sizeof(TDS_POOL));
	/* FIX ME -- read this from the conf file */
	pool_read_conf_file(name, pool);
	pool->num_members = pool->max_open_conn;

	pool->name = (char *) malloc(strlen(name)+1);
	strcpy(pool->name,name); 

	pool_mbr_init(pool);
	pool_user_init(pool);

	return pool;
}

void pool_schedule_waiters(TDS_POOL *pool)
{
TDS_POOL_USER *puser;
TDS_POOL_MEMBER *pmbr;
int i, free_mbrs;

	/* first see if there are free members to do the request */
	free_mbrs = 0;
	for (i=0;i<pool->num_members;i++) {
		pmbr = (TDS_POOL_MEMBER *) &pool->members[i];
		if (pmbr->tds && pmbr->state==TDS_COMPLETED)
			free_mbrs++;
	}

	if (!free_mbrs) return;

	for (i=0;i<pool->max_users;i++) {
		puser = (TDS_POOL_USER *) &pool->users[i];
		if (puser->user_state == TDS_SRV_WAIT) {
			/* place back in query state */
			puser->user_state = TDS_SRV_QUERY;
			waiters--;
			/* now try again */
			pool_user_query(pool, puser);
			return;
		}
     }
}
/* 
** pool_main_loop
** Accept new connections from clients, and handle all input from clients and
** pool members.
*/
int pool_main_loop(TDS_POOL *pool)
{
TDS_POOL_USER *puser;
TDS_POOL_MEMBER *pmbr;
struct sockaddr_in sin;
int     s, maxfd, fd;
int     len, i;
int	retval;
fd_set rfds;

/* fix me -- read the interfaces file and bind accordingly */
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(pool->port);
	sin.sin_family = AF_INET;

	if ((s = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		perror ("socket");
		exit (1);
	}

	fprintf(stderr,"Listening on port %d\n",pool->port);
	if (bind (s, (struct sockaddr *) &sin, sizeof (sin)) < 0) {
		perror("bind");
		exit (1);
	}
	listen (s, 5);

 	FD_ZERO(&rfds);
	FD_SET(s, &rfds);
	maxfd = s;

	while (!term) {
		fprintf(stderr,"waiting for a connect\n");
		retval = select(maxfd+1, &rfds, NULL, NULL, NULL);
		if (term) continue;

		/* process the sockets */
		if (FD_ISSET(s, &rfds)) { 
			puser=pool_user_create(pool,s,&sin);
		}
		pool_process_users(pool,&rfds);
		pool_process_members(pool,&rfds);
		/* back from members */
		if (waiters) {
			pool_schedule_waiters(pool);
		}
		
 		FD_ZERO(&rfds);
		/* add the listening socket to the read list */
		FD_SET(s, &rfds);
		maxfd = s;

		/* add the user sockets to the read list */
		for (i=0;i<pool->max_users;i++) {
			puser = (TDS_POOL_USER *) &pool->users[i];
			/* skip dead connections */
			if (puser->tds) {
				if (puser->tds->s > maxfd) maxfd = puser->tds->s;
				FD_SET(puser->tds->s, &rfds);
			}
		}

		/* add the pool member sockets to the read list */
		for (i=0;i<pool->num_members;i++) {
			pmbr = (TDS_POOL_MEMBER *) &pool->members[i];
			if (pmbr->tds) {
				if (pmbr->tds->s > maxfd) maxfd = pmbr->tds->s;
				FD_SET(pmbr->tds->s, &rfds);
			}
		}
	} /* while !term */
	close(s);
	for (i=0;i<pool->max_users;i++) {
		puser = (TDS_POOL_USER *) &pool->users[i];
		if (!IS_TDSDEAD(puser->tds)) {
			fprintf(stderr,"Closing user %d\n",i);	
			tds_close_socket(puser->tds);
		}
	}
	for (i=0;i<pool->num_members;i++) {
		pmbr = (TDS_POOL_MEMBER *) &pool->members[i];
		if (!IS_TDSDEAD(pmbr->tds) {
			fprintf(stderr,"Closing member %d\n",i);	
			tds_close_socket(pmbr->tds);
		}
	}
}

main(int argc, char **argv)
{
TDS_POOL *pool;

	signal(SIGTERM, term_handler);
	signal(SIGINT, term_handler);
	if (argc<2) {
		fprintf(stderr,"Usage: tdspool <pool name>\n");
		exit(1);
	}
	pool = pool_init(argv[1]);
	pool_main_loop(pool);
	fprintf(stdout,"tdspool Shutdown\n");
}


