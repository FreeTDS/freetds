/* $Id: server.h,v 1.1 2002-09-20 21:08:35 castellano Exp $ */

/* login.c */
void tds_read_login(TDSSOCKET *tds, TDSLOGIN *login);

/* server.c */
void tds_env_change(TDSSOCKET *tds,int type, char *oldvalue, char *newvalue);
void tds_send_msg(TDSSOCKET *tds,int msgno, int msgstate, int severity,
        char *msgtext, char *srvname, char *procname, int line);
void tds_send_login_ack(TDSSOCKET *tds, char *progname);
void tds_send_253_token(TDSSOCKET *tds, TDS_TINYINT flags, TDS_INT numrows);
