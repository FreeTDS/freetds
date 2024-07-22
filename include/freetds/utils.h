/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2016 Frediano Ziglio
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

#ifndef _tdsguard_eFYZwccrMRZEhF1ruz6j9O_
#define _tdsguard_eFYZwccrMRZEhF1ruz6j9O_

#if HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */

#include <freetds/time.h>
#include <freetds/sysdep_private.h>

#include <freetds/pushvis.h>

#ifdef __cplusplus
extern "C" {
#endif

void tds_sleep_s(unsigned sec);
void tds_sleep_ms(unsigned ms);

char *tds_getpassarg(char *arg);

char *tds_timestamp_str(char *str, int maxlen);
struct tm *tds_localtime_r(const time_t *timep, struct tm *result);
int tds_getservice(const char *name);

int tds_socket_set_nosigpipe(TDS_SYS_SOCKET sock, int on);
int tds_socket_set_nodelay(TDS_SYS_SOCKET sock);

char *tds_strndup(const void *s, TDS_INTPTR len);

#ifdef __cplusplus
}
#endif

#include <freetds/popvis.h>

#endif
