#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <ctpublic.h>
#include "common.h"

static CS_RETCODE ex_servermsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_SERVERMSG * errmsg);
static CS_RETCODE ex_clientmsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_CLIENTMSG * errmsg);

static int verbose = 0;

static CS_CONTEXT *ctx;
static CS_CONNECTION *conn;
static CS_COMMAND *cmd;
static CS_COMMAND *cmd2;

static void
cleanup(void)
{
	CS_RETCODE ret;

	if (verbose) {
		printf("Trying logout\n");
	}

	if (cmd2)
		ct_cmd_drop(cmd2);

	ret = try_ctlogout(ctx, conn, cmd, verbose);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "Logout failed\n");
		exit(1);
	}
}

static void
chk(int check, const char *fmt, ...)
{
	va_list ap;

	if (check)
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	cleanup();
	exit(1);
}

int
main(int argc, char *argv[])
{
	int errCode = 1;

	CS_RETCODE ret;
	CS_RETCODE results_ret;
	CS_CHAR cmdbuf[4096];
	CS_CHAR name[257];
	CS_INT datalength;
	CS_SMALLINT ind;
	CS_INT count;
	CS_INT num_cols;
	CS_INT row_count = 0;
	CS_DATAFMT datafmt;
	CS_DATAFMT descfmt;
	CS_INT intvar;
	CS_INT intvarsize;
	CS_SMALLINT intvarind;
	CS_INT res_type;

	int i;

	if (argc > 1 && (0 == strcmp(argv[1], "-v")))
		verbose = 1;

	printf("%s: use ct_dynamic to prepare and execute  a statement\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	ret = try_ctlogin(&ctx, &conn, &cmd, verbose);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "Login failed\n");
		return 1;
	}

	ct_callback(ctx, NULL, CS_SET, CS_CLIENTMSG_CB, (CS_VOID *) ex_clientmsg_cb);

	ct_callback(ctx, NULL, CS_SET, CS_SERVERMSG_CB, (CS_VOID *) ex_servermsg_cb);

	ret = ct_cmd_alloc(conn, &cmd2);
	chk(ret == CS_SUCCEED, "cmd2_alloc failed\n");

	/* do not test error */
	ret = run_command(cmd, "IF OBJECT_ID('tempdb..#ct_dynamic') IS NOT NULL DROP table #ct_dynamic");

	strcpy(cmdbuf, "create table #ct_dynamic (id numeric identity not null, \
        name varchar(30), age int, cost money, bdate datetime, fval float) ");

	ret = run_command(cmd, cmdbuf);
	chk(ret == CS_SUCCEED, "create table failed\n");

	strcpy(cmdbuf, "insert into #ct_dynamic ( name , age , cost , bdate , fval ) ");
	strcat(cmdbuf, "values ('Bill', 44, 2000.00, 'May 21 1960', 60.97 ) ");

	ret = run_command(cmd, cmdbuf);
	chk(ret == CS_SUCCEED, "insert table failed\n");

	strcpy(cmdbuf, "insert into #ct_dynamic ( name , age , cost , bdate , fval ) ");
	strcat(cmdbuf, "values ('Freddy', 32, 1000.00, 'Jan 21 1972', 70.97 ) ");

	ret = run_command(cmd, cmdbuf);
	chk(ret == CS_SUCCEED, "insert table failed\n");

	strcpy(cmdbuf, "insert into #ct_dynamic ( name , age , cost , bdate , fval ) ");
	strcat(cmdbuf, "values ('James', 42, 5000.00, 'May 21 1962', 80.97 ) ");

	ret = run_command(cmd, cmdbuf);
	chk(ret == CS_SUCCEED, "insert table failed\n");

	strcpy(cmdbuf, "select name from #ct_dynamic where age = ?");

	ret = ct_dynamic(cmd, CS_PREPARE, "age", CS_NULLTERM, cmdbuf, CS_NULLTERM);
	chk(ret == CS_SUCCEED, "ct_dynamic failed\n");

	chk(ct_send(cmd) == CS_SUCCEED, "ct_send(CS_PREPARE) failed\n");

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			break;

		default:
			goto ERR;
		}
	}
	chk(ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) ret);

	ret = ct_dynamic(cmd, CS_DESCRIBE_INPUT, "age", CS_NULLTERM, NULL, CS_UNUSED);
	chk(ret == CS_SUCCEED, "ct_dynamic failed\n");

	chk(ct_send(cmd) == CS_SUCCEED, "ct_send(CS_DESCRIBE_INPUT) failed\n");

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_DESCRIBE_RESULT:
			ret = ct_res_info(cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL);
			chk(ret == CS_SUCCEED, "ct_res_info() failed");

			for (i = 1; i <= num_cols; i++) {
				ret = ct_describe(cmd, i, &descfmt);
				chk(ret == CS_SUCCEED, "ct_describe() failed");
				fprintf(stderr, "CS_DESCRIBE_INPUT parameter %d :\n", i);
				if (descfmt.namelen == 0)
					fprintf(stderr, "\t\tNo name...\n");
				else
					fprintf(stderr, "\t\tName = %*.*s\n", descfmt.namelen, descfmt.namelen, descfmt.name);
				fprintf(stderr, "\t\tType   = %d\n", descfmt.datatype);
				fprintf(stderr, "\t\tLength = %d\n", descfmt.maxlength);
			}
			break;

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			break;

		default:
			goto ERR;
		}
	}

	ret = ct_dynamic(cmd, CS_DESCRIBE_OUTPUT, "age", CS_NULLTERM, NULL, CS_UNUSED);
	chk(ret == CS_SUCCEED, "ct_dynamic failed\n");

	chk(ct_send(cmd) == CS_SUCCEED, "ct_send(CS_DESCRIBE_OUTPUT) failed\n");

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_DESCRIBE_RESULT:
			ret = ct_res_info(cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL);
			chk(ret == CS_SUCCEED, "ct_res_info() failed");
			chk(num_cols == 1, "CS_DESCRIBE_OUTPUT showed %d columns , expected 1\n", num_cols);

			for (i = 1; i <= num_cols; i++) {
				ret = ct_describe(cmd, i, &descfmt);
				chk(ret == CS_SUCCEED, "ct_describe() failed");

				if (descfmt.namelen == 0)
					fprintf(stderr, "\t\tNo name...\n");
				else
					fprintf(stderr, "\t\tName = %*.*s\n", descfmt.namelen, descfmt.namelen, descfmt.name);
				fprintf(stderr, "\t\tType   = %d\n", descfmt.datatype);
				fprintf(stderr, "\t\tLength = %d\n", descfmt.maxlength);
			}
			break;

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			break;

		default:
			goto ERR;
		}
	}

	/* execute dynamic on a second command to check it still works */
	ret = ct_dynamic(cmd2, CS_EXECUTE, "age", CS_NULLTERM, NULL, CS_UNUSED);
	chk(ret == CS_SUCCEED, "ct_dynamic failed\n");

	intvar = 44;
	intvarsize = 4;
	intvarind = 0;

	datafmt.name[0] = 0;
	datafmt.namelen = 0;
	datafmt.datatype = CS_INT_TYPE;
	datafmt.status = CS_INPUTVALUE;

	ret = ct_setparam(cmd2, &datafmt, (CS_VOID *) & intvar, &intvarsize, &intvarind);
	chk(ret == CS_SUCCEED, "ct_setparam(int) failed\n");

	chk(ct_send(cmd2) == CS_SUCCEED, "ct_send(CS_EXECUTE) failed\n");

	while ((results_ret = ct_results(cmd2, &res_type)) == CS_SUCCEED) {
		chk(res_type != CS_CMD_FAIL, "1: ct_results() result_type CS_CMD_FAIL.\n");
		chk(res_type != CS_COMPUTE_RESULT, "ct_results() unexpected CS_COMPUTE_RESULT.\n");

		switch ((int) res_type) {
		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_ROW_RESULT:
			datafmt.datatype = CS_CHAR_TYPE;
			datafmt.format = CS_FMT_NULLTERM;
			datafmt.maxlength = 256;
			datafmt.count = 1;
			datafmt.locale = NULL;
			ret = ct_bind(cmd2, 1, &datafmt, name, &datalength, &ind);
			chk(ret == CS_SUCCEED, "ct_bind() failed\n");

			while (((ret = ct_fetch(cmd2, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				chk(ret != CS_ROW_FAIL, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
				if (ret == CS_SUCCEED) {
					chk(!strcmp(name, "Bill"), "fetched value '%s' expected 'Bill'\n", name);
				} else {
					break;
				}
			}
			chk(ret == CS_END_DATA, "ct_fetch() unexpected return %d.\n", (int) ret);
			break;
		default:
			fprintf(stderr, "ct_results() unexpected result_type.\n");
			return 1;
		}
	}
	chk(results_ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) results_ret);

	intvar = 32;

	chk(ct_send(cmd2) == CS_SUCCEED, "ct_send(CS_EXECUTE) failed\n");

	while ((results_ret = ct_results(cmd2, &res_type)) == CS_SUCCEED) {
		chk(res_type != CS_CMD_FAIL, "2: ct_results() result_type CS_CMD_FAIL.\n");
		chk(res_type != CS_COMPUTE_RESULT, "ct_results() unexpected CS_COMPUTE_RESULT.\n");

		switch ((int) res_type) {
		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_ROW_RESULT:
			datafmt.datatype = CS_CHAR_TYPE;
			datafmt.format = CS_FMT_NULLTERM;
			datafmt.maxlength = 256;
			datafmt.count = 1;
			datafmt.locale = NULL;
			ret = ct_bind(cmd2, 1, &datafmt, name, &datalength, &ind);
			chk(ret == CS_SUCCEED, "ct_bind() failed\n");

			while (((ret = ct_fetch(cmd2, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
			       || (ret == CS_ROW_FAIL)) {
				row_count += count;
				chk(ret != CS_ROW_FAIL, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);

				if (ret == CS_SUCCEED) {
					chk(!strcmp(name, "Freddy"), "fetched value '%s' expected 'Freddy'\n", name);
				} else {
					break;
				}
			}
			chk(ret == CS_END_DATA, "ct_fetch() unexpected return %d.\n", (int) ret);
			break;
		default:
			fprintf(stderr, "ct_results() unexpected result_type.\n");
			return 1;
		}
	}
	chk(results_ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) results_ret);

	ret = ct_dynamic(cmd, CS_DEALLOC, "age", CS_NULLTERM, NULL, CS_UNUSED);
	chk(ret == CS_SUCCEED, "ct_dynamic failed\n");

	chk(ct_send(cmd) == CS_SUCCEED, "ct_send(CS_DEALLOC) failed\n");

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			break;

		default:
			goto ERR;
		}
	}
	chk(ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) ret);

	/*
	 * check we can prepare again dynamic with same name after deallocation
	 */
	strcpy(cmdbuf, "select name from #ct_dynamic where age = ?");
	ret = ct_dynamic(cmd, CS_PREPARE, "age", CS_NULLTERM, cmdbuf, CS_NULLTERM);
	chk(ret == CS_SUCCEED, "ct_dynamic failed\n");

	chk(ct_send(cmd) == CS_SUCCEED, "ct_send(CS_PREPARE) failed\n");

	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		switch ((int) res_type) {

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;

		case CS_CMD_FAIL:
			break;

		default:
			goto ERR;
		}
	}
	chk(ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) ret);

	ct_dynamic(cmd, CS_DEALLOC, "invalid", CS_NULLTERM, NULL, CS_UNUSED);
	ct_send(cmd);
	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED)
		chk(res_type != CS_ROW_RESULT, "Rows not expected\n");
	chk(ret == CS_END_RESULTS, "ct_results() unexpected return.\n", (int) ret);

	ret = ct_dynamic(cmd2, CS_EXECUTE, "age", CS_NULLTERM, NULL, CS_UNUSED);
	chk(ret == CS_SUCCEED, "ct_dynamic failed\n");

	intvar = 32;
	intvarsize = 4;
	intvarind = 0;

	datafmt.name[0] = 0;
	datafmt.namelen = 0;
	datafmt.datatype = CS_INT_TYPE;
	datafmt.status = CS_INPUTVALUE;

	ret = ct_setparam(cmd2, &datafmt, (CS_VOID *) & intvar, &intvarsize, &intvarind);
	chk(ct_send(cmd2) == CS_SUCCEED, "ct_send(CS_EXECUTE) failed\n");

	/* all tests succeeded */
	errCode = 0;

      ERR:
	cleanup();
	return errCode;
}


static CS_RETCODE
ex_clientmsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_CLIENTMSG * errmsg)
{
	printf("\nOpen Client Message:\n");
	printf("Message number: LAYER = (%d) ORIGIN = (%d) ", CS_LAYER(errmsg->msgnumber), CS_ORIGIN(errmsg->msgnumber));
	printf("SEVERITY = (%d) NUMBER = (%d)\n", CS_SEVERITY(errmsg->msgnumber), CS_NUMBER(errmsg->msgnumber));
	printf("Message String: %s\n", errmsg->msgstring);
	if (errmsg->osstringlen > 0) {
		printf("Operating System Error: %s\n", errmsg->osstring);
	}
	fflush(stdout);

	return CS_SUCCEED;
}

static CS_RETCODE
ex_servermsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_SERVERMSG * srvmsg)
{
	printf("\nServer message:\n");
	printf("Message number: %d, Severity %d, ", srvmsg->msgnumber, srvmsg->severity);
	printf("State %d, Line %d\n", srvmsg->state, srvmsg->line);

	if (srvmsg->svrnlen > 0) {
		printf("Server '%s'\n", srvmsg->svrname);
	}

	if (srvmsg->proclen > 0) {
		printf(" Procedure '%s'\n", srvmsg->proc);
	}

	printf("Message String: %s\n", srvmsg->text);
	fflush(stdout);

	return CS_SUCCEED;
}
