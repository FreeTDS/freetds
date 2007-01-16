/*	$Id: handlers.h,v 1.1 2007-01-16 21:33:12 castellano Exp $	*/
extern int global_errorlevel;

int err_handler(DBPROCESS *dbproc, int severity, int dberr, 
		int oserr, char *dberrstr, char *oserrstr);

int msg_handler(DBPROCESS *dbproc, DBINT msgno, int msgstate, 
		int severity, char *msgtext, char *srvname, 
		char *procname, int line);
