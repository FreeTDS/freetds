/*
 * Tests rows count
 *
 * These are the results using Sybase CT-Library. This test check that ct_results returns the same results.
 *
 *
 * 	                "insert into #tmp1 values(1) "
 * 			"insert into #tmp1 values(2) "
 * 			"insert into #tmp1 values(3) "
 * 			"select * from #tmp1 "
 *
 * Open Client Message:
 * number 16843050 layer 1 origin 1 severity 1 number 42
 * msgstring: ct_res_info(ROWCOUNT): user api layer: external error: This routine cannot be called after ct_results() returns a result type of CS_ROW_RESULT.
 * osstring: (null)
 * ct_results returned CS_ROW_RESULT type and -1 rows
 * All done processing rows.
 * ct_results returned CS_CMD_DONE type and 3 rows
 *
 * Open Client Message:
 * number 16843051 layer 1 origin 1 severity 1 number 43
 * msgstring: ct_res_info(ROWCOUNT): user api layer: external error: This routine cannot be called after ct_results() returns a result type of CS_STATUS_RESULT.
 * osstring: (null)
 * ct_results returned CS_STATUS_RESULT type and -1 rows
 * All done processing rows.
 * ct_results returned CS_CMD_SUCCEED type and -1 rows
 * ct_results returned CS_CMD_DONE type and -1 rows
 *
 *
 * 	                "insert into #tmp1 values(1) "
 * 			"insert into #tmp1 values(2) "
 * 			"insert into #tmp1 values(3) "
 * 			"select * from #tmp1 "
 * 			"insert into #tmp1 values(4) "
 * 			"delete from #tmp1 where i <= 2 "
 *
 * Open Client Message:
 * number 16843050 layer 1 origin 1 severity 1 number 42
 * msgstring: ct_res_info(ROWCOUNT): user api layer: external error: This routine cannot be called after ct_results() returns a result type of CS_ROW_RESULT.
 * osstring: (null)
 * ct_results returned CS_ROW_RESULT type and -1 rows
 * All done processing rows.
 * ct_results returned CS_CMD_DONE type and 3 rows
 *
 * Open Client Message:
 * number 16843051 layer 1 origin 1 severity 1 number 43
 * msgstring: ct_res_info(ROWCOUNT): user api layer: external error: This routine cannot be called after ct_results() returns a result type of CS_STATUS_RESULT.
 * osstring: (null)
 * ct_results returned CS_STATUS_RESULT type and -1 rows
 * All done processing rows.
 * ct_results returned CS_CMD_SUCCEED type and 2 rows
 * ct_results returned CS_CMD_DONE type and 2 rows
 */

#undef NDEBUG
#include <config.h>

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <assert.h>
#include <ctpublic.h>
#include "common.h"
#include "replacements.h"

static CS_CONTEXT *ctx;
static CS_CONNECTION *conn;
static CS_COMMAND *cmd;

static CS_RETCODE ex_clientmsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_CLIENTMSG * errmsg);
static CS_RETCODE ex_servermsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_SERVERMSG * errmsg);
static CS_INT ex_display_results(CS_COMMAND * cmd, char *results);

static int test(int final_rows, int no_rows);

int
main(int argc, char *argv[])
{
	CS_RETCODE ret;

	printf("%s: check row count returned\n", __FILE__);
	ret = try_ctlogin(&ctx, &conn, &cmd, 0);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "Login failed\n");
		return 1;
	}

	ct_callback(ctx, NULL, CS_SET, CS_CLIENTMSG_CB, (CS_VOID *) ex_clientmsg_cb);
	ct_callback(ctx, NULL, CS_SET, CS_SERVERMSG_CB, (CS_VOID *) ex_servermsg_cb);

	/* do not test error */
	run_command(cmd, "DROP PROCEDURE sample_rpc");
	run_command(cmd, "drop table #tmp1");

	/* test with rows from select */
	if (test(0, 0) || test(1, 0))
		return 1;

	/* test with empty select */
	if (test(0, 1) || test(1, 1))
		return 1;

	ret = try_ctlogout(ctx, conn, cmd, 0);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "Logout failed\n");
		return 1;
	}

	return 0;
}

static int
test(int final_rows, int no_rows)
{
	CS_RETCODE ret;
	CS_CHAR cmdbuf[4096];
	char results[1024];

	run_command(cmd, "create table #tmp1 (i int not null)");

	results[0] = 0;
	strcpy(cmdbuf, "create proc sample_rpc as ");

	strcat(results, "CS_ROW_RESULT -1\n");
	strcat(cmdbuf,  "insert into #tmp1 values(1) "
			"insert into #tmp1 values(2) "
			"insert into #tmp1 values(3) "
	);

	if (no_rows) {
		strcat(cmdbuf,  "select * from #tmp1 where i > 10 ");
		strcat(results, "CS_CMD_DONE 0\n");
	} else {
		strcat(cmdbuf,  "select * from #tmp1 ");
		strcat(results, "CS_CMD_DONE 3\n");
	}

	strcat(results, "CS_STATUS_RESULT -1\n");

	if (final_rows) {
		strcat(cmdbuf,  "insert into #tmp1 values(4) "
				"delete from #tmp1 where i <= 2 "
		);
		strcat(results, "CS_CMD_SUCCEED 2\n"
			        "CS_CMD_DONE 2\n");
	} else {
		strcat(results, "CS_CMD_SUCCEED -1\n"
			        "CS_CMD_DONE -1\n");
	}

	printf("testing query:\n----\n%s\n----\n", cmdbuf);

	ret = run_command(cmd, cmdbuf);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "create proc failed\n");
		return 1;
	}

	printf("----------\n");
	if ((ret = ct_command(cmd, CS_RPC_CMD, "sample_rpc", CS_NULLTERM, CS_NO_RECOMPILE)) != CS_SUCCEED) {
		fprintf(stderr, "ct_command(CS_RPC_CMD) failed");
		return 1;
	}

	if (ct_send(cmd) != CS_SUCCEED) {
		fprintf(stderr, "ct_send(RPC) failed");
		return 1;
	}
	ex_display_results(cmd, results);

	/* cleanup */
	run_command(cmd, "DROP PROCEDURE sample_rpc");
	run_command(cmd, "drop table #tmp1");

	return 0;
}

static CS_INT
ex_display_results(CS_COMMAND * cmd, char *results)
{
	CS_RETCODE ret;
	CS_INT res_type;
	CS_INT num_cols;
	CS_INT row_count = 0;
	CS_INT rows_read;
	CS_SMALLINT msg_id;

	/*
	 * Process the results of the RPC.
	 */
	while ((ret = ct_results(cmd, &res_type)) == CS_SUCCEED) {
		char res[32];
		int rows, pos;

		CS_INT rowsAffected = -1;
		ct_res_info(cmd, CS_ROW_COUNT, &rowsAffected, CS_UNUSED, NULL);
		printf("ct_results returned %s type and %d rows\n", res_type_str(res_type), (int) rowsAffected);

		/* check expected results are the same as got ones */
		pos = -1;
		assert(sscanf(results, "%30s %d %n", res, &rows, &pos) >= 2 && pos > 0);
		results += pos;
		if (strcmp(res_type_str(res_type), res) != 0 || rowsAffected != rows) {
			fprintf(stderr, "Expected ct_results %s rows %d\n", res, rows);
			exit(1);
		}

		switch (res_type) {
		case CS_ROW_RESULT:
		case CS_PARAM_RESULT:
		case CS_STATUS_RESULT:

			/*
			 * All three of these result types are fetchable.
			 * Since the result model for rpcs and rows have
			 * been unified in the New Client-Library, we
			 * will use the same routine to display them
			 */

			/*
			 * Find out how many columns there are in this result set.
			 */
			ret = ct_res_info(cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL);
			if (ret != CS_SUCCEED) {
				fprintf(stderr, "ct_res_info(CS_NUMDATA) failed");
				return 1;
			}

			/*
			 * Make sure we have at least one column
			 */
			if (num_cols <= 0) {
				fprintf(stderr, "ct_res_info(CS_NUMDATA) returned zero columns");
				return 1;
			}

			while (((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED,
						&rows_read)) == CS_SUCCEED) || (ret == CS_ROW_FAIL)) {
				/*
				 * Increment our row count by the number of rows just fetched.
				 */
				row_count = row_count + rows_read;

				/*
				 * Check if we hit a recoverable error.
				 */
				if (ret == CS_ROW_FAIL) {
					fprintf(stdout, "Error on row %d.\n", row_count);
					fflush(stdout);
				}
			}

			/*
			 * We're done processing rows.  Let's check the final return
			 * value of ct_fetch().
			 */
			switch ((int) ret) {
			case CS_END_DATA:
				/*
				 * Everything went fine.
				 */
				printf("All done processing rows.\n");
				fflush(stdout);
				break;

			case CS_FAIL:
				/*
				 * Something terrible happened.
				 */
				fprintf(stderr, "ct_fetch returned CS_FAIL\n");
				return 1;
				break;

			default:
				/*
				 * We got an unexpected return value.
				 */
				fprintf(stderr, "ct_fetch returned %d\n", ret);
				return 1;
				break;

			}
			break;

		case CS_MSG_RESULT:
			ret = ct_res_info(cmd, CS_MSGTYPE, (CS_VOID *) & msg_id, CS_UNUSED, NULL);
			if (ret != CS_SUCCEED) {
				fprintf(stderr, "ct_res_info(msg_id) failed");
				return 1;
			}
			printf("ct_result returned CS_MSG_RESULT where msg id = %d.\n", msg_id);
			fflush(stdout);
			break;

		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
		case CS_CMD_FAIL:
			break;

		default:
			/*
			 * We got something unexpected.
			 */
			fprintf(stderr, "ct_results returned unexpected result type.");
			return CS_FAIL;
		}
	}

	/*
	 * We're done processing results. Let's check the
	 * return value of ct_results() to see if everything
	 * went ok.
	 */
	switch ((int) ret) {
	case CS_END_RESULTS:
		/*
		 * Everything went fine.
		 */
		break;

	case CS_FAIL:
		/*
		 * Something failed happened.
		 */
		fprintf(stderr, "ct_results failed.");
		break;

	default:
		/*
		 * We got an unexpected return value.
		 */
		fprintf(stderr, "ct_results returned unexpected result type.");
		break;
	}

	return CS_SUCCEED;
}

static CS_RETCODE
ex_clientmsg_cb(CS_CONTEXT * context, CS_CONNECTION * connection, CS_CLIENTMSG * errmsg)
{
	printf("\nOpen Client Message:\n");
	printf("Message number: LAYER = (%ld) ORIGIN = (%ld) ", (long) CS_LAYER(errmsg->msgnumber), (long) CS_ORIGIN(errmsg->msgnumber));
	printf("SEVERITY = (%ld) NUMBER = (%ld)\n", (long) CS_SEVERITY(errmsg->msgnumber), (long) CS_NUMBER(errmsg->msgnumber));
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
	printf("Message number: %ld, Severity %ld, ", (long) srvmsg->msgnumber, (long) srvmsg->severity);
	printf("State %ld, Line %ld\n", (long) srvmsg->state, (long) srvmsg->line);

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
