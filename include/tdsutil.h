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
"$Id: tdsutil.h,v 1.3 2002-08-16 16:45:20 freddy77 Exp $";
static void *no_unused_tdsutil_h_warn[]={rcsid_tdsutil_h, no_unused_tdsutil_h_warn};


#define MAXPATH 256

extern TDS_SMALLINT tds_get_smallint(TDSSOCKET *tds);
extern unsigned char tds_get_byte(TDSSOCKET *tds);
extern unsigned char tds_peek(TDSSOCKET *tds);
extern void tds_unget_byte(TDSSOCKET *tds);
extern char *tds_get_n(TDSSOCKET *tds, void *dest, int n);
extern int tds_read_packet (TDSSOCKET *tds);
extern int set_interfaces_file_loc(char *interfloc);
extern int get_server_info(char *server, char *ip_addr, char *ip_port, char *tds_ver);
extern int get_size_by_type(int servertype);
extern int tds_flush_packet(TDSSOCKET *tds);
extern int tds_send_login(TDSSOCKET *tds, TDSCONFIGINFO *config);
extern int tds_process_login_tokens(TDSSOCKET *tds);
extern int tds_put_buf(TDSSOCKET *tds, const unsigned char *buf, int dsize, int ssize);

extern void tds_free_compute_results(TDSCOMPUTEINFO *comp_info);

extern int  tdsdump_open(const char *filename);
extern void tdsdump_off();
extern void tdsdump_on();
extern void tdsdump_close();
extern void tdsdump_log(int dbg_lvl, const char *fmt, ...);

extern int  tds_is_result_row(TDSSOCKET *tds);
extern int  tds_is_result_set(TDSSOCKET *tds);
extern int  tds_is_end_of_results(TDSSOCKET *tds);
extern int  tds_is_error(TDSSOCKET *tds);
extern int  tds_is_message(TDSSOCKET *tds);
extern int  tds_is_doneinproc(TDSSOCKET *tds);
extern int  tds_is_control(TDSSOCKET *tds);

extern int  tds_msleep(long usecs);
/* added 'cause used but not declared (mlilback, 11/7/01) */
extern TDS_INT tds_get_int(TDSSOCKET *tds);

#ifdef __cplusplus
#if 0
{ /* keep the paren matcher and indenter happy */
#endif 
}
#endif

#endif

