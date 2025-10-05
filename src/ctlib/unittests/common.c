#include "common.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <freetds/replacements.h>
#include <freetds/macros.h>

#ifdef TDS_STATIC_CAST
#include "ctlib.h"
#endif

static const char *BASENAME = NULL;

static const char *PWD = DEFAULT_PWD_PATH;

int cslibmsg_cb_invoked = 0;
int clientmsg_cb_invoked = 0;
int servermsg_cb_invoked = 0;
bool error_to_stdout = false;

ct_message ct_last_message;

static CS_RETCODE continue_logging_in(CS_CONTEXT ** ctx, CS_CONNECTION ** conn, CS_COMMAND ** cmd, int verbose);

CS_RETCODE
read_login_info(void)
{
	const char *s = read_login_info_base(&common_pwd, DEFAULT_PWD_PATH);

	return s ? CS_SUCCEED : CS_FAIL;
}

static CS_RETCODE
establish_login(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	int ch;

	BASENAME = basename((char *)argv[0]);

	/* process command line options (handy for manual testing) */
	while ((ch = getopt(argc, argv, "U:P:S:D:f:m:v")) != -1) {
		switch (ch) {
		case 'U':
			strlcpy(common_pwd.user, optarg, sizeof(common_pwd.user));
			break;
		case 'P':
			strlcpy(common_pwd.password, optarg, sizeof(common_pwd.password));
			break;
		case 'S':
			strlcpy(common_pwd.server, optarg, sizeof(common_pwd.server));
			break;
		case 'D':
			strlcpy(common_pwd.database, optarg, sizeof(common_pwd.database));
			break;
		case 'f': /* override default PWD file */
			PWD = strdup(optarg);
			break;
		case 'm':
			common_pwd.maxlength = (CS_INT) strtol(optarg, NULL, 10);
		case 'v':
			common_pwd.fverbose = 1;
			break;
		case '?':
		default:
			fprintf(stderr, "usage:  %s [-v] [-f PWD]\n"
					"        [-U username] [-P password]\n"
					"        [-S servername] [-D database]\n"
					, BASENAME);
			exit(1);
		}
	}
	read_login_info();

	/* user is not necessary for MSSQL trusted logins */
	return (*common_pwd.server && *common_pwd.database) ? CS_SUCCEED : CS_FAIL;
}

CS_RETCODE
try_ctlogin_with_options(int argc, char **argv, CS_CONTEXT ** ctx, CS_CONNECTION ** conn, CS_COMMAND ** cmd, int verbose)
{
	CS_RETCODE ret;

	if ((ret = establish_login(argc, argv)) != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "read_login_info() failed!\n");
		}
		return ret;
	}
	return continue_logging_in(ctx, conn, cmd, verbose);
}

/* old way: because I'm too lazy to change every unit test */
CS_RETCODE
try_ctlogin(CS_CONTEXT ** ctx, CS_CONNECTION ** conn, CS_COMMAND ** cmd, int verbose)
{
	CS_RETCODE ret;

	if ((ret = read_login_info()) != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "read_login_info() failed!\n");
		}
		return ret;
	}
	return continue_logging_in(ctx, conn, cmd, verbose);
}

CS_RETCODE
continue_logging_in(CS_CONTEXT ** ctx, CS_CONNECTION ** conn, CS_COMMAND ** cmd, int verbose)
{
	CS_RETCODE ret;
	char query[512+10];
	CS_INT bulk_enabled = CS_TRUE;
#ifdef TDS_STATIC_CAST
	TDSCONTEXT *tds_ctx;
#endif

	ret = cs_ctx_alloc(CS_VERSION_100, ctx);
	if (ret != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "Context Alloc failed!\n");
		}
		return ret;
	}

#ifdef TDS_STATIC_CAST
	/* Force default date format, some tests rely on it */
	tds_ctx = (TDSCONTEXT *) (*ctx)->tds_ctx;
	if (tds_ctx && tds_ctx->locale && tds_ctx->locale->datetime_fmt) {
		free(tds_ctx->locale->datetime_fmt);
		tds_ctx->locale->datetime_fmt = strdup("%b %d %Y %I:%M%p");
	}
#endif

	ret = ct_init(*ctx, CS_VERSION_100);
	if (ret != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "Library Init failed!\n");
		}
		return ret;
	}
	if ((ret = ct_callback(*ctx, NULL, CS_SET, CS_CLIENTMSG_CB,
			       (CS_VOID*) clientmsg_cb)) != CS_SUCCEED) {
		fprintf(stderr, "ct_callback() failed\n");
		return ret;
	}
	if ((ret = ct_callback(*ctx, NULL, CS_SET, CS_SERVERMSG_CB, servermsg_cb)) != CS_SUCCEED) {
		fprintf(stderr, "ct_callback() failed\n");
		return ret;
	}
	ret = ct_con_alloc(*ctx, conn);
	if (ret != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "Connect Alloc failed!\n");
		}
		return ret;
	}
	ret = ct_con_props(*conn, CS_SET, CS_USERNAME, common_pwd.user, CS_NULLTERM, NULL);
	if (ret != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "ct_con_props() SET USERNAME failed!\n");
		}
		return ret;
	}
	ret = ct_con_props(*conn, CS_SET, CS_PASSWORD, common_pwd.password, CS_NULLTERM, NULL);
	if (ret != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "ct_con_props() SET PASSWORD failed!\n");
		}
		return ret;
	}
	ret = ct_con_props(*conn, CS_SET, CS_BULK_LOGIN, &bulk_enabled, CS_UNUSED, NULL);
	if (ret != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "ct_con_props() SET BULK failed!\n");
		}
		return ret;
	}

	printf("connecting as %s to %s.%s\n", common_pwd.user, common_pwd.server, common_pwd.database);

	ret = ct_connect(*conn, common_pwd.server, CS_NULLTERM);
	if (ret != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "Connection failed!\n");
		}
		ct_con_drop(*conn);
		*conn = NULL;
		ct_exit(*ctx, CS_UNUSED);
		cs_ctx_drop(*ctx);
		*ctx = NULL;
		return ret;
	}
	ret = ct_cmd_alloc(*conn, cmd);
	if (ret != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "Command Alloc failed!\n");
		}
		ct_con_drop(*conn);
		*conn = NULL;
		ct_exit(*ctx, CS_UNUSED);
		cs_ctx_drop(*ctx);
		*ctx = NULL;
		return ret;
	}

	sprintf(query, "use %s", common_pwd.database);

	ret = run_command(*cmd, query);
	if (ret != CS_SUCCEED)
		return ret;

	return CS_SUCCEED;
}


CS_RETCODE
try_ctlogout(CS_CONTEXT * ctx, CS_CONNECTION * conn, CS_COMMAND * cmd, int verbose)
{
	CS_RETCODE ret;

	ret = ct_cancel(conn, NULL, CS_CANCEL_ALL);
	if (ret != CS_SUCCEED) {
		if (verbose) {
			fprintf(stderr, "ct_cancel() failed!\n");
		}
		return ret;
	}
	ct_cmd_drop(cmd);
	ct_close(conn, CS_UNUSED);
	ct_con_drop(conn);
	ct_exit(ctx, CS_UNUSED);
	cs_ctx_drop(ctx);

	return CS_SUCCEED;
}

/* Run commands from which we expect no results returned */
CS_RETCODE
run_command(CS_COMMAND * cmd, const char *sql)
{
	CS_RETCODE ret, results_ret;
	CS_INT result_type;

	if (cmd == NULL) {
		return CS_FAIL;
	}

	ret = ct_command(cmd, CS_LANG_CMD, (void *) sql, CS_NULLTERM, CS_UNUSED);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "ct_command() failed\n");
		return ret;
	}
	ret = ct_send(cmd);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "ct_send() failed\n");
		return ret;
	}
	ret = CS_SUCCEED;
	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch (result_type) {
		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_CMD_FAIL:
			fprintf(stderr, "ct_results() result_type CS_CMD_FAIL.\n");
			ret = CS_FAIL;
			break;
		default:
			fprintf(stderr, "ct_results() unexpected result_type.\n");
			return CS_FAIL;
		}
	}
	switch (results_ret) {
	case CS_END_RESULTS:
		break;
	case CS_FAIL:
		fprintf(stderr, "ct_results() failed.\n");
		return CS_FAIL;
		break;
	default:
		fprintf(stderr, "ct_results() unexpected return.\n");
		return CS_FAIL;
	}

	return ret;
}

CS_INT
cslibmsg_cb(CS_CONTEXT * connection TDS_UNUSED, CS_CLIENTMSG * errmsg)
{
	cslibmsg_cb_invoked++;

	ct_reset_last_message();
	ct_last_message.type = CTMSG_CSLIB;
	ct_last_message.number = errmsg->msgnumber;
	strlcpy(ct_last_message.text, errmsg->msgstring, sizeof(ct_last_message.text));

	fprintf(stderr, "\nCS-Library Message:\n");
	fprintf(stderr, "number %#x layer %d origin %d severity %d number %d\n",
		errmsg->msgnumber,
		(int) CS_LAYER(errmsg->msgnumber), (int) CS_ORIGIN(errmsg->msgnumber),
		(int) CS_SEVERITY(errmsg->msgnumber), (int) CS_NUMBER(errmsg->msgnumber));
	fprintf(stderr, "msgstring: %s\n", errmsg->msgstring);
	fprintf(stderr, "osstring: %s\n", (errmsg->osstringlen > 0)
		? errmsg->osstring : "(null)");
	return CS_SUCCEED;
}



static CS_RETCODE
clientmsg_gen_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_CLIENTMSG * errmsg,
		 ct_message_type msg_type)
{
	FILE *out = error_to_stdout ? stdout: stderr;

	clientmsg_cb_invoked++;

	ct_reset_last_message();
	ct_last_message.type = msg_type;
	ct_last_message.number = errmsg->msgnumber;
	strlcpy(ct_last_message.text, errmsg->msgstring, sizeof(ct_last_message.text));

	fprintf(out, "\nOpen Client Message%s: %p %p\n",
		msg_type == CTMSG_CLIENT ? "" : " #2", context, connection);
	fprintf(out, "number %#x layer %d origin %d severity %d number %d\n",
		errmsg->msgnumber,
		(int) CS_LAYER(errmsg->msgnumber), (int) CS_ORIGIN(errmsg->msgnumber),
		(int) CS_SEVERITY(errmsg->msgnumber), (int) CS_NUMBER(errmsg->msgnumber));
	fprintf(out, "msgstring: %s\n", errmsg->msgstring);
	fprintf(out, "osstring: %s\n", (errmsg->osstringlen > 0)
		? errmsg->osstring : "(null)");
	fflush(out);
	return CS_SUCCEED;
}

CS_RETCODE
clientmsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_CLIENTMSG * errmsg)
{
	return clientmsg_gen_cb(context, connection, errmsg, CTMSG_CLIENT);
}

CS_RETCODE
clientmsg_cb2(CS_CONTEXT * context, CS_CONNECTION * connection, CS_CLIENTMSG * errmsg)
{
	return clientmsg_gen_cb(context, connection, errmsg, CTMSG_CLIENT2);
}

CS_RETCODE
servermsg_cb(CS_CONTEXT * context TDS_UNUSED, CS_CONNECTION * connection TDS_UNUSED, CS_SERVERMSG * srvmsg)
{
	FILE *out = error_to_stdout ? stdout: stderr;

	servermsg_cb_invoked++;

	ct_reset_last_message();
	ct_last_message.type = CTMSG_SERVER;
	ct_last_message.number = srvmsg->msgnumber;
	strlcpy(ct_last_message.text, srvmsg->text, sizeof(ct_last_message.text));

	if (srvmsg->msgnumber == 5701 || srvmsg->msgnumber == 5703) {
		fprintf(out, "%s\n", srvmsg->text);
		fflush(out);
		return CS_SUCCEED;
	}

	fprintf(out, "\nServer message:\n");
	fprintf(out, "%s Message %d severity %d state %d line %d:\n",
		srvmsg->svrnlen > 0? srvmsg->svrname : "Server",
		srvmsg->msgnumber, srvmsg->severity, srvmsg->state, srvmsg->line);
	if (srvmsg->proclen > 0)
		fprintf(out, "proc %s: ", srvmsg->proc);
	fprintf(out, "\t\"%s\"\n", srvmsg->text);

	fflush(out);
	return CS_SUCCEED;
}

const char *
res_type_str(CS_RETCODE ret)
{
	static char str[64];

#define S(s) case s: return #s;
	switch ((int) ret) {
	S(CS_ROW_RESULT)
	S(CS_PARAM_RESULT)
	S(CS_STATUS_RESULT)
	S(CS_MSG_RESULT)
	S(CS_CMD_SUCCEED)
	S(CS_CMD_DONE)
	S(CS_CMD_FAIL)
	S(CS_COMPUTE_RESULT)
#undef S
	}

	sprintf(str, "?? (%d)", (int) ret);
	return str;
}

void
_check_ret(const char *name, CS_RETCODE ret, int line)
{
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "%s():%d: failed\n", name, line);
		exit(1);
	}
}

void
ct_reset_last_message(void)
{
	memset(&ct_last_message, 0, sizeof(ct_last_message));
}
