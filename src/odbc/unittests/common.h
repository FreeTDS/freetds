#ifdef WIN32
#include <windows.h>
#include <direct.h>
#endif

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

#include <sqltypes.h>
#include <sql.h>
#include <sqlext.h>

static char rcsid_common_h[] = "$Id: common.h,v 1.12 2004-02-14 18:52:15 freddy77 Exp $";
static void *no_unused_common_h_warn[] = { rcsid_common_h, no_unused_common_h_warn };

extern HENV Environment;
extern HDBC Connection;
extern HSTMT Statement;
extern int use_odbc_version3;

extern char USER[512];
extern char SERVER[512];
extern char PASSWORD[512];
extern char DATABASE[512];
extern char DRIVER[1024];

int read_login_info(void);
void CheckReturn(void);
int Connect(void);
int Disconnect(void);
void Command(HSTMT stmt, const char *command);
SQLRETURN CommandWithResult(HSTMT stmt, const char *command);

#if !HAVE_SETENV
void odbc_setenv(const char* name, const char *value, int overwrite);
#define setenv odbc_setenv
#endif

