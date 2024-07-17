/* Test we can insert long binary into database.
 */
#include "common.h"

#include <freetds/macros.h>

static const CS_INT unused = CS_UNUSED, nullterm = CS_NULLTERM;
static CS_INT result_len = -1;

static CS_RETCODE
csmsg_callback(CS_CONTEXT *ctx TDS_UNUSED, CS_CLIENTMSG * emsgp)
{
	printf("message from csmsg_callback(): %s\n", emsgp->msgstring);
	return CS_SUCCEED;
}

static int
fetch_results(CS_COMMAND * command)
{
	CS_INT result_type, int_result, copied, rows_read;
	CS_SMALLINT ind = 0;
	CS_DATAFMT datafmt;
	int result = 0;

	memset(&datafmt, 0, sizeof(datafmt));
	datafmt.datatype = CS_INT_TYPE;
	datafmt.count = 1;

	check_call(ct_results, (command, &result_type));
	do {
		if (result_type == CS_ROW_RESULT) {
			check_call(ct_bind, (command, 1, &datafmt, &int_result, &copied, &ind));
			check_call(ct_fetch, (command, CS_UNUSED, CS_UNUSED, CS_UNUSED, &rows_read));
			printf("received %d bytes\n", (int) int_result);
			result_len = int_result;
		}
		if (result_type == CS_CMD_FAIL) {
			result = 1;
		}
	} while (ct_results(command, &result_type) == CS_SUCCEED);
	return result;
}

static int
execute_sql(CS_COMMAND * command, const char *sql)
{
	printf("executing sql: %s\n", sql);
	check_call(ct_command, (command, CS_LANG_CMD, sql, nullterm, unused));
	check_call(ct_send, (command));
	return fetch_results(command);
}

TEST_MAIN()
{
	int verbose = 0;
	CS_COMMAND *command;
	CS_CONNECTION *connection;
	CS_CONTEXT *context;
	unsigned char buffer[65536];
	CS_INT buffer_len = 8192;
	CS_SMALLINT ind = 0;
	CS_DATAFMT datafmt;
	CS_INT ret;
	int i;

	printf("-- begin --\n");

	check_call(try_ctlogin, (&context, &connection, &command, verbose));
	check_call(cs_config, (context, CS_SET, CS_MESSAGE_CB, (CS_VOID *) csmsg_callback, unused, 0));

	execute_sql(command, "if object_id('mps_table') is not null drop table mps_table");
	execute_sql(command, "if object_id('mps_rpc') is not null drop procedure mps_rpc");
	/* if this query fails probably we are using wrong database vendor or version */
	ret = execute_sql(command, "create procedure mps_rpc (@varbinary_param varbinary(max)) as "
		    "insert mps_table values (@varbinary_param) " "select len(varbinary_data) from mps_table");
	if (ret != 0) {
		try_ctlogout(context, connection, command, verbose);
		return 0;
	}
	execute_sql(command, "create table mps_table (varbinary_data varbinary(max))");

	if (argc > 1)
		buffer_len = atoi(argv[1]);
	if (buffer_len < 0 || buffer_len > sizeof(buffer))
		return 1;

	printf("sending %d bytes\n", buffer_len);

	for (i = 0; i < buffer_len; i++)
		buffer[i] = (rand() % 16);

	memset(&datafmt, 0, sizeof(datafmt));
	strcpy(datafmt.name, "@varbinary_param");
	datafmt.namelen = nullterm;
	datafmt.datatype = CS_IMAGE_TYPE;
	datafmt.status = CS_INPUTVALUE;

	check_call(ct_command, (command, CS_RPC_CMD, "mps_rpc", nullterm, unused));
	check_call(ct_setparam, (command, &datafmt, buffer, &buffer_len, &ind));
	check_call(ct_send, (command));

	fetch_results(command);

	execute_sql(command, "drop table mps_table");
	execute_sql(command, "drop procedure mps_rpc");
	try_ctlogout(context, connection, command, verbose);

	printf("-- end --\n");
	return (result_len == buffer_len) ? 0 : 1;
}
