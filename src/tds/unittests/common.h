#ifndef COMMON_h
#define COMMON_h

static char rcsid_common_h[] = "$Id: common.h,v 1.7 2003-11-15 09:30:45 freddy77 Exp $";
static void *no_unused_common_h_warn[] = { rcsid_common_h, no_unused_common_h_warn };

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <tds.h>

extern char PASSWORD[512];
extern char USER[512];
extern char SERVER[512];
extern char DATABASE[512];
extern char CHARSET[512];

extern TDSCONTEXT *test_context;

int try_tds_login(TDSLOGIN ** login, TDSSOCKET ** tds, const char *appname, int verbose);
int try_tds_logout(TDSLOGIN * login, TDSSOCKET * tds, int verbose);

int run_query(TDSSOCKET * tds, const char *query);

#endif
