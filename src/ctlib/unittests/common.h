
#ifndef COMMON_h
#define COMMON_h

static char  rcsid_common_h [ ] =
         "$Id: common.h,v 1.2 2002-09-16 21:00:57 castellano Exp $";
static void *no_unused_common_h_warn[]={rcsid_common_h, no_unused_common_h_warn};

extern char PASSWORD[512];
extern char USER[512];
extern char SERVER[512];
extern char DATABASE[512];

CS_RETCODE try_ctlogin(CS_CONTEXT **ctx, CS_CONNECTION **conn, CS_COMMAND **cmd, int verbose);
CS_RETCODE try_ctlogout(CS_CONTEXT *ctx, CS_CONNECTION *conn, CS_COMMAND *cmd, int verbose);
CS_RETCODE run_command(CS_COMMAND *cmd, char *sql);

#endif
