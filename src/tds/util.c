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
#include <stdarg.h>

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

TDS_RCSID(var, "$Id: util.c,v 1.68 2006-08-08 14:43:51 freddy77 Exp $");

/* for now all messages go to the log */
int tds_debug_flags = TDS_DBGFLAG_ALLLVL | TDS_DBGFLAG_SOURCE;
int tds_g_append_mode = 0;
static char *g_dump_filename = NULL;
static int write_dump = 0;	/* is TDS stream debug log turned on? */
static FILE *g_dumpfile = NULL;	/* file pointer for dump log          */
static TDS_MUTEX_DECLARE(g_dump_mutex);

static FILE* tdsdump_append(void);

#ifdef TDS_ATTRIBUTE_DESTRUCTOR
static void __attribute__((destructor))
tds_util_deinit(void)
{
	tdsdump_close();
}
#endif

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

		tds->query_start_time = time(NULL);

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

/**
 * Temporarily turn off logging.
 */
void
tdsdump_off(void)
{
	TDS_MUTEX_LOCK(&g_dump_mutex);
	write_dump = 0;
	TDS_MUTEX_UNLOCK(&g_dump_mutex);
}				/* tdsdump_off()  */


/**
 * Turn logging back on.  You must call tdsdump_open() before calling this routine.
 */
void
tdsdump_on(void)
{
	TDS_MUTEX_LOCK(&g_dump_mutex);
	write_dump = 1;
	TDS_MUTEX_UNLOCK(&g_dump_mutex);
}				/* tdsdump_on()  */


/**
 * This creates and truncates a human readable dump file for the TDS
 * traffic.  The name of the file is specified by the filename
 * parameter.  If that is given as NULL or an empty string,
 * any existing log file will be closed.
 *
 * \return  true if the file was opened, false if it couldn't be opened.
 */
int
tdsdump_open(const char *filename)
{
	int result;		/* really should be a boolean, not an int */

	TDS_MUTEX_LOCK(&g_dump_mutex);

	/* same append file */
	if (tds_g_append_mode && filename != NULL && g_dump_filename != NULL && strcmp(filename, g_dump_filename) == 0) {
		TDS_MUTEX_UNLOCK(&g_dump_mutex);
		return 1;
	}

	/* free old one */
	if (g_dumpfile != NULL && g_dumpfile != stdout && g_dumpfile != stderr)
		fclose(g_dumpfile);
	g_dumpfile = NULL;
	if (g_dump_filename)
		TDS_ZERO_FREE(g_dump_filename);

	/* required to close just log ?? */
	if (filename == NULL || filename[0] == '\0') {
		TDS_MUTEX_UNLOCK(&g_dump_mutex);
		return 1;
	}

	result = 1;
	if (tds_g_append_mode) {
		g_dump_filename = strdup(filename);
		/* if mutex are available do not reopen file every time */
#ifdef TDS_HAVE_PTHREAD_MUTEX
		g_dumpfile = tdsdump_append();
#endif
	} else if (!strcmp(filename, "stdout")) {
		g_dumpfile = stdout;
	} else if (!strcmp(filename, "stderr")) {
		g_dumpfile = stderr;
	} else if (NULL == (g_dumpfile = fopen(filename, "w"))) {
		result = 0;
	}

	if (result)
		write_dump = 1;
	TDS_MUTEX_UNLOCK(&g_dump_mutex);

	if (result) {
		char today[64];
		struct tm *tm;
		time_t t;

		time(&t);
		tm = localtime(&t);

		strftime(today, sizeof(today), "%Y-%m-%d %H:%M:%S", tm);
		tdsdump_log(TDS_DBG_INFO1, "Starting log file for FreeTDS %s\n"
			    "\ton %s with debug flags 0x%x.\n", VERSION, today, tds_debug_flags);
	}
	return result;
}				/* tdsdump_open()  */

static FILE*
tdsdump_append(void)
{
	if (!g_dump_filename)
		return NULL;

	if (!strcmp(g_dump_filename, "stdout")) {
		return stdout;
	} else if (!strcmp(g_dump_filename, "stderr")) {
		return stderr;
	}
	return fopen(g_dump_filename, "a");
}


/**
 * Close the TDS dump log file.
 */
void
tdsdump_close(void)
{
	TDS_MUTEX_LOCK(&g_dump_mutex);
	write_dump = 0;
	if (g_dumpfile != NULL && g_dumpfile != stdout && g_dumpfile != stderr)
		fclose(g_dumpfile);
	g_dumpfile = NULL;
	if (g_dump_filename)
		TDS_ZERO_FREE(g_dump_filename);
	TDS_MUTEX_UNLOCK(&g_dump_mutex);
}				/* tdsdump_close()  */

static void
tdsdump_start(FILE *file, const char *fname, int line)
{
	char buf[128], *pbuf;
	int started = 0;

	/* write always time before log */
	if (tds_debug_flags & TDS_DBGFLAG_TIME) {
		fputs(tds_timestamp_str(buf, 127), file);
		started = 1;
	}

	pbuf = buf;
	if (tds_debug_flags & TDS_DBGFLAG_PID) {
		if (started)
			*pbuf++ = ' ';
		pbuf += sprintf(pbuf, "%d", (int) getpid());
		started = 1;
	}

	if ((tds_debug_flags & TDS_DBGFLAG_SOURCE) && fname && line) {
		const char *p;
		p = strrchr(fname, '/');
		if (p)
			fname = p + 1;
		p = strrchr(fname, '\\');
		if (p)
			fname = p + 1;
		if (started)
			pbuf += sprintf(pbuf, " (%s:%d)", fname, line);
		else
			pbuf += sprintf(pbuf, "%s:%d", fname, line);
		started = 1;
	}
	if (started)
		*pbuf++ = ':';
	*pbuf = 0;
	fputs(buf, file);
}

/**
 * Dump the contents of data into the log file in a human readable format.
 * \param msg      message to print before dump
 * \param buf      buffer to dump
 * \param length   number of bytes in the buffer
 */
void
tdsdump_dump_buf(const char* file, unsigned int level_line, const char *msg, const void *buf, int length)
{
	int i;
	int j;
#define BYTES_PER_LINE 16
	const unsigned char *data = (const unsigned char *) buf;
	const int debug_lvl = level_line & 15;
	const int line = level_line >> 4;
	char line_buf[BYTES_PER_LINE * 8 + 16], *p;
	FILE *dumpfile;

	if (((tds_debug_flags >> debug_lvl) & 1) == 0 || !write_dump)
		return;

	TDS_MUTEX_LOCK(&g_dump_mutex);

	dumpfile = g_dumpfile;
#ifdef TDS_HAVE_PTHREAD_MUTEX
	if (tds_g_append_mode && dumpfile == NULL)
		dumpfile = g_dumpfile = tdsdump_append();
#else
	if (tds_g_append_mode)
		dumpfile = tdsdump_append();
#endif

	if (dumpfile == NULL) {
		TDS_MUTEX_UNLOCK(&g_dump_mutex);
		return;
	}

	tdsdump_start(dumpfile, file, line);

	fprintf(dumpfile, "%s\n", msg);

	for (i = 0; i < length; i += BYTES_PER_LINE) {
		p = line_buf;
		/*
		 * print the offset as a 4 digit hex number
		 */
		p += sprintf(p, "%04x", i);

		/*
		 * print each byte in hex
		 */
		for (j = 0; j < BYTES_PER_LINE; j++) {
			if (j == BYTES_PER_LINE / 2)
				*p++ = '-';
			else
				*p++ = ' ';
			if (j + i >= length)
				p += sprintf(p, "  ");
			else
				p += sprintf(p, "%02x", data[i + j]);
		}

		/*
		 * skip over to the ascii dump column
		 */
		p += sprintf(p, " |");

		/*
		 * print each byte in ascii
		 */
		for (j = i; j < length && (j - i) < BYTES_PER_LINE; j++) {
			if (j - i == BYTES_PER_LINE / 2)
				*p++ = ' ';
			p += sprintf(p, "%c", (isprint(data[j])) ? data[j] : '.');
		}
		strcpy(p, "|\n");
		fputs(line_buf, dumpfile);
	}
	fputs("\n", dumpfile);

	fflush(dumpfile);

#ifndef TDS_HAVE_PTHREAD_MUTEX
	if (tds_g_append_mode) {
		if (dumpfile != stdout && dumpfile != stderr)
			fclose(dumpfile);
	}
#endif

	TDS_MUTEX_UNLOCK(&g_dump_mutex);

}				/* tdsdump_dump_buf()  */


/**
 * This function write a message to the debug log.  
 * \param debug_lvl level of debugging
 * \param fmt       printf-like format string
 */
void
tdsdump_log(const char* file, unsigned int level_line, const char *fmt, ...)
{
	const int debug_lvl = level_line & 15;
	const int line = level_line >> 4;
	va_list ap;
	FILE *dumpfile;

	if (((tds_debug_flags >> debug_lvl) & 1) == 0 || !write_dump)
		return;

	TDS_MUTEX_LOCK(&g_dump_mutex);

	dumpfile = g_dumpfile;
#ifdef TDS_HAVE_PTHREAD_MUTEX
	if (tds_g_append_mode && dumpfile == NULL)
		dumpfile = g_dumpfile = tdsdump_append();
#else
	if (tds_g_append_mode)
		dumpfile = tdsdump_append();
#endif
	
	if (dumpfile == NULL) { 
		TDS_MUTEX_UNLOCK(&g_dump_mutex);
		return;
	}

	tdsdump_start(dumpfile, file, line);

	va_start(ap, fmt);

	vfprintf(dumpfile, fmt, ap);
	va_end(ap);

	fflush(dumpfile);

#ifndef TDS_HAVE_PTHREAD_MUTEX
	if (tds_g_append_mode) {
		if (dumpfile != stdout && dumpfile != stderr)
			fclose(dumpfile);
	}
#endif
	TDS_MUTEX_UNLOCK(&g_dump_mutex);
}				/* tdsdump_log()  */

