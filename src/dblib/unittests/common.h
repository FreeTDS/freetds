
#ifndef COMMON_h
#define COMMON_h

static char rcsid_common_h[] = "$Id: common.h,v 1.8 2004-07-22 13:37:55 freddy77 Exp $";
static void *no_unused_common_h_warn[] = { rcsid_common_h, no_unused_common_h_warn };

extern char PASSWORD[512];
extern char USER[512];
extern char SERVER[512];
extern char DATABASE[512];

void set_malloc_options(void);
int read_login_info(int argc, char **argv);
void check_crumbs(void);
void add_bread_crumb(void);
int syb_msg_handler(DBPROCESS * dbproc,
		    DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line);
int syb_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);

#endif
