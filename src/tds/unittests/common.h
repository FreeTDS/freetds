
#ifndef COMMON_h
#define COMMON_h

static char  rcsid_common_h [ ] =
         "$Id: common.h,v 1.2 2002-09-16 20:28:02 castellano Exp $";
static void *no_unused_common_h_warn[]={rcsid_common_h, no_unused_common_h_warn};

extern char PASSWORD[512];
extern char USER[512];
extern char SERVER[512];
extern char DATABASE[512];

int try_tds_login(TDSLOGIN  **login, TDSSOCKET **tds, char *appname, int verbose);
int try_tds_logout(TDSLOGIN  *login, TDSSOCKET *tds, int verbose);

#endif
