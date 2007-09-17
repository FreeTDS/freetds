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

TDS_RCSID(var, "$Id: util.c,v 1.81 2007-09-17 21:27:30 jklowden Exp $");

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
	const TDS_STATE prior_state = tds->state;
	static const char state_names[][10] = {
		"IDLE",
	        "QUERYING",
	        "PENDING",
	        "READING",
	        "DEAD"
	};
	assert(state < TDS_VECTOR_SIZE(state_names));
	assert(tds->state < TDS_VECTOR_SIZE(state_names));
	
	if (state == tds->state)
		return state;
	
	switch(state) {
		/* transition to READING are valid only from PENDING */
	case TDS_PENDING:
		if (tds->state != TDS_READING && tds->state != TDS_QUERYING) {
			tdsdump_log(TDS_DBG_ERROR, "logic error: cannot change query state from %s to %s\n", 
							state_names[prior_state], state_names[state]);
			return tds->state;
		}
		tds->state = state;
		break;
	case TDS_READING:
		if (tds->state != TDS_PENDING) {
			tdsdump_log(TDS_DBG_ERROR, "logic error: cannot change query state from %s to %s\n", 
							state_names[prior_state], state_names[state]);
			return tds->state;
		}
		tds->state = state;
		break;
	case TDS_IDLE:
		if (tds->state == TDS_DEAD && TDS_IS_SOCKET_INVALID(tds->s)) {
			tdsdump_log(TDS_DBG_ERROR, "logic error: cannot change query state from %s to %s\n",
				state_names[prior_state], state_names[state]);
			return tds->state;
		}
	case TDS_DEAD:
		tds->state = state;
		break;
	case TDS_QUERYING:
		CHECK_TDS_EXTRA(tds);

		if (tds->state == TDS_DEAD) {
			tdsdump_log(TDS_DBG_ERROR, "logic error: cannot change query state from %s to %s\n", 
							state_names[prior_state], state_names[state]);
			tdserror(tds->tds_ctx, tds, TDSEWRIT, 0);
			break;
		} else if (tds->state != TDS_IDLE) {
			tdsdump_log(TDS_DBG_ERROR, "logic error: cannot change query state from %s to %s\n", 
							state_names[prior_state], state_names[state]);
			tdserror(tds->tds_ctx, tds, TDSERPND, 0);
			break;
		}

		/* TODO check this code, copied from tds_submit_prepare */
		tds_free_all_results(tds);
		tds->rows_affected = TDS_NO_COUNT;
		tds_release_cursor(tds, tds->cur_cursor);
		tds->cur_cursor = NULL;
		tds->internal_sp_called = 0;

		tds->state = state;
		break;
	default:
		assert(0);
		break;
	}
	
	tdsdump_log(TDS_DBG_ERROR, "Changed query state from %s to %s\n", state_names[prior_state], state_names[state]);
	CHECK_TDS_EXTRA(tds);
	
	return tds->state; 
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

/*
 * Call the client library's error handler
 */
#define EXINFO         1
#define EXUSER         2
#define EXNONFATAL     3
#define EXCONVERSION   4
#define EXSERVER       5
#define EXTIME         6
#define EXPROGRAM      7
#define EXRESOURCE     8
#define EXCOMM         9
#define EXFATAL       10
#define EXCONSISTENCY 11

typedef struct _tds_error_message 
{
	TDSERRNO msgno;
	int severity;
	char *msgtext;
} TDS_ERROR_MESSAGE;

static const TDS_ERROR_MESSAGE tds_error_messages[] = 
	{ { TDSEICONVIU,     EXCONVERSION,	"Buffer exhausted converting characters from client into server's character set" }
	, { TDSEICONVAVAIL,  EXCONVERSION,	"Character set conversion is not available between client character set '%.*s' and "
						"server character set '%.*s'" }
	, { TDSEICONVO,      EXCONVERSION,	"Error converting characters into server's character set. Some character(s) could "
						"not be converted" }
	, { TDSEICONVI,      EXCONVERSION,	"Some character(s) could not be converted into client's character set.  "
						"Unconverted bytes were changed to question marks ('?')" }
	, { TDSEICONV2BIG,   EXCONVERSION,	"Some character(s) could not be converted into client's character set" }
	, { TDSERPND,           EXPROGRAM,	"Attempt to initiate a new Adaptive Server operation with results pending" }
	, { TDSEBTOK,              EXCOMM,	"Bad token from the server: Datastream processing out of sync" }
	, { TDSECAP,               EXCOMM,	"DB-Library capabilities not accepted by the Server" }
	, { TDSECAPTYP,            EXCOMM,	"Unexpected capability type in CAPABILITY datastream" }
	, { TDSECLOS,              EXCOMM,	"Error in closing network connection" }
	, { TDSECONN,              EXCOMM,	"Unable to connect: Adaptive Server is unavailable or does not exist" }
	, { TDSEEUNR,              EXCOMM,	"Unsolicited event notification received" }
	, { TDSEFCON,              EXCOMM,	"Adaptive Server connection failed" }
	, { TDSENEG,               EXCOMM,	"Negotiated login attempt failed" }
	, { TDSEOOB,               EXCOMM,	"Error in sending out-of-band data to the server" }
	, { TDSEREAD,              EXCOMM,	"Read from the server failed" }
	, { TDSESEOF,              EXCOMM,	"Unexpected EOF from the server" }
	, { TDSESOCK,              EXCOMM,	"Unable to open socket" }
	, { TDSESYNC,              EXCOMM,	"Read attempted while out of synchronization with Adaptive Server" }
	, { TDSEUMSG,              EXCOMM,	"Unknown message-id in MSG datastream" }
	, { TDSEUSCT,              EXCOMM,	"Unable to set communications timer" }
	, { TDSEUTDS,              EXCOMM,	"Unrecognized TDS version received from the server" }
	, { TDSEWRIT,              EXCOMM,	"Write to the server failed" }
	/* last, with masgno == 0 */
	, { 0,              EXCONSISTENCY,	"unrecognized msgno" }
	};
	
static
const char * retname(int retcode)
{
	switch(retcode) {
	case TDS_INT_CONTINUE:
		return "TDS_INT_CONTINUE";
	case TDS_INT_CANCEL:
		return "TDS_INT_CANCEL";
	case TDS_INT_TIMEOUT:
		return "TDS_INT_TIMEOUT";
	}
	assert(0);
	return "nonesuch";
}

/**
 * \brief Call the client library's error handler (for library-generated errors only)
 *
 * The client library error handler may return: 
 * TDS_INT_CANCEL -- Return TDS_FAIL to the calling function.  For TDSETIME, closes the connection first. 
 * TDS_INT_CONTINUE -- For TDSETIME only, retry the network read/write operation. Else invalid.
 * TDS_INT_TIMEOUT -- For TDSETIME only, send a TDSCANCEL packet. Else invalid.
 *
 * These are Sybase semantics, but they serve all purposes.  
 * The application tells the library to quit, fail, retry, or attempt to cancel.  In the event of a network timeout, 
 * a failed operation necessarily means the connection becomes unusable, because no cancellation dialog was 
 * concluded with the server.  
 *
 * It is the client library's duty to call the error handler installed by the application, if any, and to interpret the
 * installed handler's return code.  It may return to this function one of the above codes only.  This function will not 
 * check the return code because there's nothing that can be done here except abort.  It is merely passed to the 
 * calling function, which will (we hope) DTRT.  
 *
 * \param tds_ctx	points to a TDSCONTEXT structure
 * \param tds		the connection structure, may be NULL if not connected
 * \param msgno		an enumerated libtds msgno, cf. tds.h
 * \param errnum	the OS errno, if it matters, else zero
 * 
 * \returns client library function's return code
 */
int
tdserror (const TDSCONTEXT * tds_ctx, TDSSOCKET * tds, int msgno, int errnum)
{
#if 0
	static const char int_exit_text[] = "FreeTDS: libtds: exiting because client error handler returned %d for msgno %d\n";
	static const char int_invalid_text[] = "%s (%d) received from client library error handler for nontimeout for error %d."
					       "  Treating as INT_EXIT\n";
#endif
	const TDS_ERROR_MESSAGE *err;

	TDSMESSAGE msg;
	int rc = TDS_INT_CANCEL;
	char *os_msgtext = strerror(errnum);

	tdsdump_log(TDS_DBG_FUNC, "tdserror(%p, %p, %d, %d)\n", tds_ctx, tds, msgno, errnum);

	if (os_msgtext == NULL)
		os_msgtext = "no OS error";

	/* look up the error message */
	for (err = tds_error_messages; err->msgno; ++err) {
		if (err->msgno == msgno)
			break;
	}


	CHECK_CONTEXT_EXTRA(tds_ctx);

	if (tds)
		CHECK_TDS_EXTRA(tds);

	if (tds_ctx && tds_ctx->err_handler) {
		memset(&msg, 0, sizeof(TDSMESSAGE));
		msg.msgno = msgno;
		msg.severity = err->severity;
		msg.state = -1;
		msg.server = "OpenClient";
		msg.line_number = -1;
		msg.message = err->msgtext;
		msg.sql_state = tds_alloc_client_sqlstate(msg.msgno);

		/*
		 * Call client library handler.
		 * The client library must return a valid code.  It is not checked again here.
		 */
		rc = tds_ctx->err_handler(tds_ctx, tds, &msg);

		TDS_ZERO_FREE(msg.sql_state);
	}

 	tdsdump_log(TDS_DBG_FUNC, "tdserror: client library returned %s(%d)\n", retname(rc), rc);
  
  	assert(!(msgno != TDSETIME && rc == TDS_INT_TIMEOUT));   /* client library should prevent */
	assert(!(msgno != TDSETIME && rc == TDS_INT_CONTINUE));  /* client library should prevent */
	
	if (msgno != TDSETIME) {
		switch(rc) {
		case TDS_INT_TIMEOUT:
		case TDS_INT_CONTINUE:
		 	tdsdump_log(TDS_DBG_FUNC, "exit: %s(%d) valid only for TDSETIME\n", retname(rc), rc);
			exit(1);
		default:
			break;
		}
	}

 	if (rc == TDS_INT_TIMEOUT) {
 		tds_send_cancel(tds);
 		rc = TDS_INT_CONTINUE;
 	} 

 	tdsdump_log(TDS_DBG_FUNC, "tdserror: returning %s(%d)\n", retname(rc), rc);
  
	return rc;
}




