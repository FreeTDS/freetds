#ifndef _tdsguard_dxzbHKJsR5HKI0Qpzu7Vnq_
#define _tdsguard_dxzbHKJsR5HKI0Qpzu7Vnq_

#undef NDEBUG
#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/tds.h>
#include <freetds/data.h>

#ifndef FREETDS_SRCDIR
#define FREETDS_SRCDIR FREETDS_TOPDIR "/src/tds/unittests"
#endif

extern char PASSWORD[512];
extern char USER[512];
extern char SERVER[512];
extern char DATABASE[512];
extern char CHARSET[512];

extern TDSCONTEXT *test_context;

int try_tds_login(TDSLOGIN ** login, TDSSOCKET ** tds, const char *appname, int verbose);
int try_tds_logout(TDSLOGIN * login, TDSSOCKET * tds, int verbose);

int run_query(TDSSOCKET * tds, const char *query);

extern int utf8_max_len;

int get_unichar(const char **psrc);
char *to_utf8(const char *src, char *dest);

typedef void tds_any_type_t(TDSSOCKET *tds, TDSCOLUMN *col);
void tds_all_types(TDSSOCKET *tds, tds_any_type_t *func);

#endif
