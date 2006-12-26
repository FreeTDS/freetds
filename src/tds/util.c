/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdarg.h>

#if TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef WIN32
#include <process.h>
#endif

#include "tds.h"
#include "tds_checks.h"
#include "tdsthread.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: util.c,v 1.71 2006-12-26 12:53:02 freddy77 Exp $");

void
tds_set_parent(TDSSOCKET * tds, void *the_parent)
{
	if (tds)
		tds->parent = the_parent;
}

void *
tds_get_parent(TDSSOCKET * tds)
{
	return (tds->parent);
}

/**
 * Set state of TDS connection, with logging and checking.
 * \param tds	  state information for the socket and the TDS protocol
 * \param state	  the new state of the connection, cf. TDS_STATE.
 * \return 	  the new state, which might not be \a state.
 */
TDS_STATE
tds_set_state(TDSSOCKET * tds, TDS_STATE state)
{
	static const char state_names[][10] = {
		"IDLE",
	        "QUERYING",
	        "PENDING",
	        "READING",
	        "DEAD"
	};
	assert(state < TDS_VECTOR_SIZE(state_names));
	assert(tds->state < TDS_VECTOR_SIZE(state_names));
	
	tdsdump_log(TDS_DBG_ERROR, "Changing query state from %s to %s\n", state_names[tds->state], state_names[state]);
	
	switch(state) {
		/* transition to READING are valid only from PENDING */
	case TDS_PENDING:
		if (tds->state != TDS_READING && tds->state != TDS_QUERYING)
			break;
		return tds->state = state;
	case TDS_READING:
		if (tds->state != TDS_PENDING)
			break;
		return tds->state = state;
	case TDS_IDLE:
	case TDS_DEAD:
		return tds->state = state;
		break;
	default:
		assert(0);
		break;
	case TDS_QUERYING:
		CHECK_TDS_EXTRA(tds);

		if (tds->state == TDS_DEAD) {
			tds_client_msg(tds->tds_ctx, tds, 20006, 9, 0, 0, "Write to SQL Server failed.");
			return tds->state;
		} else if (tds->state != TDS_IDLE) {
			tdsdump_log(TDS_DBG_ERROR, "tds_submit_query(): state is PENDING\n");
			tds_client_msg(tds->tds_ctx, tds, 20019, 7, 0, 1,
				       "Attempt to initiate a new SQL Server operation with results pending.");
			return tds->state;
		}

		tds->query_start_time_ms = tds_gettime_ms();

		/* TODO check this code, copied from tds_submit_prepare */
		tds_free_all_results(tds);
		tds->rows_affected = TDS_NO_COUNT;
		tds_release_cursor(tds, tds->cur_cursor);
		tds->cur_cursor = NULL;
		tds->internal_sp_called = 0;

		return tds->state = state;
	}
	
	return tds->state; /* should not reach here */
}


int
tds_swap_bytes(unsigned char *buf, int bytes)
{
	unsigned char tmp;
	int i;

	/* if (bytes % 2) { return 0 }; */
	for (i = 0; i < bytes / 2; i++) {
		tmp = buf[i];
		buf[i] = buf[bytes - i - 1];
		buf[bytes - i - 1] = tmp;
	}
	return bytes;
}

/**
 * Returns the version of the TDS protocol in effect for the link
 * as a decimal integer.  
 *	Typical returned values are 42, 50, 70, 80.
 * Also fills pversion_string unless it is null.
 * 	Typical pversion_string values are "4.2" and "7.0".
 */
int
tds_version(TDSSOCKET * tds_socket, char *pversion_string)
{
	int iversion = 0;

	if (tds_socket) {
		iversion = 10 * tds_socket->major_version + tds_socket->minor_version;

		if (pversion_string) {
			sprintf(pversion_string, "%d.%d", tds_socket->major_version, tds_socket->minor_version);
		}
	}

	return iversion;
}

unsigned int
tds_gettime_ms(void)
{
#ifdef WIN32
	return GetTickCount();
#elif defined(HAVE_GETHRTIME)
	return (unsigned int) (gethrtime() / 1000000u);
#elif defined(HAVE_CLOCK_GETTIME) && defined(TDS_GETTIMEMILLI_CONST)
	struct timespec ts;
	clock_gettime(TDS_GETTIMEMILLI_CONST, &ts);
	return (unsigned int) (ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
#elif defined(HAVE_GETTIMEOFDAY)
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (unsigned int) (tv.tv_sec * 1000u + tv.tv_usec / 1000u);
#else
#error How to implement tds_gettime_ms ??
#endif
}

