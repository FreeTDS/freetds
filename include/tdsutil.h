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

#ifndef _tdsutil_h_
#define _tdsutil_h_

#include "tds.h"

#include <sys/ioctl.h>
#include <time.h>                  /* Jeff's hack */
#ifndef __INCvxWorksh             /* vxWorks doesn't have a sys/time.h */
#include <sys/time.h>
#endif

#ifdef __cplusplus
extern "C" {
#if 0
} /* keep the paren matcher and indenter happy */
#endif 
#endif


static char  rcsid_tdsutil_h [ ] =
"$Id: tdsutil.h,v 1.11 2002-09-30 16:03:02 castellano Exp $";
static void *no_unused_tdsutil_h_warn[]={rcsid_tdsutil_h, no_unused_tdsutil_h_warn};


TDS_SMALLINT tds_get_smallint(TDSSOCKET *tds);
unsigned char tds_get_byte(TDSSOCKET *tds);
unsigned char tds_peek(TDSSOCKET *tds);
void tds_unget_byte(TDSSOCKET *tds);
char *tds_get_n(TDSSOCKET *tds, void *dest, int n);
int tds_read_packet (TDSSOCKET *tds);
int tds_set_interfaces_file_loc(char *interfloc);
int tds_flush_packet(TDSSOCKET *tds);
int tds_send_login(TDSSOCKET *tds, TDSCONFIGINFO *config);
int tds_process_login_tokens(TDSSOCKET *tds);
int tds_put_buf(TDSSOCKET *tds, const unsigned char *buf, int dsize, int ssize);

void tds_free_compute_results(TDSCOMPUTEINFO *comp_info);

int  tdsdump_open(const char *filename);
void tdsdump_off();
void tdsdump_on();
void tdsdump_close();
void tdsdump_log(int dbg_lvl, const char *fmt, ...);
void tdsdump_dump_buf(const void *buf, int length);
		      

int  tds_is_result_row(TDSSOCKET *tds);
int  tds_is_result_set(TDSSOCKET *tds);
int  tds_is_end_of_results(TDSSOCKET *tds);
int  tds_is_error(TDSSOCKET *tds);
int  tds_is_message(TDSSOCKET *tds);
int  tds_is_doneinproc(TDSSOCKET *tds);
int  tds_is_control(TDSSOCKET *tds);

int  tds_msleep(long usecs);
/* added 'cause used but not declared (mlilback, 11/7/01) */
TDS_INT tds_get_int(TDSSOCKET *tds);

int tds_close_socket(TDSSOCKET *tds);
int tds_swap_bytes(unsigned char *buf, int bytes);
void tds_ctx_set_parent(TDSCONTEXT *ctx, void *the_parent);

#ifdef __cplusplus
#if 0
{ /* keep the paren matcher and indenter happy */
#endif 
}
#endif

#endif

