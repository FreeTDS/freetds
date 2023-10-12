
#ifndef _freetds_ctlib_common_h
#define _freetds_ctlib_common_h

#include <freetds/bool.h>

extern char SERVER[512];
extern char DATABASE[512];
extern char USER[512];
extern char PASSWORD[512];

typedef struct 
{
	int initialized;
	char SERVER[512];
	char DATABASE[512];
	char USER[512];
	char PASSWORD[512];
	char fverbose;
	int maxlength; 
} COMMON_PWD;
extern COMMON_PWD common_pwd;

typedef enum ct_message_type {
	/** no message saved */
	CTMSG_NONE,
	/** last message from clientmsg_cb */
	CTMSG_CLIENT,
	/** last message from clientmsg_cb2 */
	CTMSG_CLIENT2,
	/** last message from cslibmsg_cb */
	CTMSG_SERVER,
	/** last message from servermsg_cb */
	CTMSG_CSLIB,
} ct_message_type;

typedef struct ct_message {
	ct_message_type type;
	CS_INT number;
	char text[CS_MAX_MSG];
} ct_message;

CS_RETCODE read_login_info(void);

extern int cslibmsg_cb_invoked;
extern int clientmsg_cb_invoked;
extern int servermsg_cb_invoked;
extern bool error_to_stdout;

CS_RETCODE try_ctlogin(CS_CONTEXT ** ctx, CS_CONNECTION ** conn, CS_COMMAND ** cmd, int verbose);
CS_RETCODE try_ctlogin_with_options(int argc, char **argv, CS_CONTEXT ** ctx, CS_CONNECTION ** conn, CS_COMMAND ** cmd, 
				    int verbose);

CS_RETCODE try_ctlogout(CS_CONTEXT * ctx, CS_CONNECTION * conn, CS_COMMAND * cmd, int verbose);

CS_RETCODE run_command(CS_COMMAND * cmd, const char *sql);
CS_RETCODE cslibmsg_cb(CS_CONTEXT * connection, CS_CLIENTMSG * errmsg);
CS_RETCODE clientmsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_CLIENTMSG * errmsg);
CS_RETCODE clientmsg_cb2(CS_CONTEXT * context, CS_CONNECTION * connection, CS_CLIENTMSG * errmsg);
CS_RETCODE servermsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_SERVERMSG * srvmsg);

const char *res_type_str(CS_RETCODE ret);

void _check_ret(const char *name, CS_RETCODE ret, int line);

#define check_call(func, args) do { \
	_check_ret(#func, func args, __LINE__); \
} while(0)

/**
 * Last message received by cslibmsg_cb, clientmsg_cb, clientmsg_cb2 or servermsg_cb.
 */
extern ct_message ct_last_message;

/**
 * Clear last message, see ct_last_message.
 * Used before a function to check if that function is reporting some error.
 */
void ct_reset_last_message(void);

#endif
