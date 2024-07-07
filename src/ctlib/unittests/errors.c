/*
 * Try usage of callbacks to get errors and messages from library.
 */
#include "common.h"

#include <bkpublic.h>

#define ALL_TESTS \
	TEST(ct_callback) \
	TEST(ct_res_info) \
	TEST(ct_send) \
	TEST(cs_config) \
	TEST(blk_init) \
	TEST(cs_loc_alloc) \
	TEST(cs_loc_drop) \
	TEST(cs_locale) \
	TEST(ct_dynamic) \
	TEST(ct_connect) \
	TEST(ct_command) \
	TEST(ct_cursor) \
	TEST(ct_con_props) \
	TEST(cs_convert)

/* forward declare all tests */
#undef TEST
#define TEST(name) static void test_ ## name(void);
ALL_TESTS

static void
report_wrong_error(int line)
{
	fprintf(stderr, "%d:Wrong error type %d number %d (%#x)\n", line,
		ct_last_message.type, ct_last_message.number, ct_last_message.number);
	exit(1);
}

static CS_CONTEXT *ctx;
static CS_CONNECTION *conn;
static CS_COMMAND *cmd;

int
main(void)
{
	int verbose = 1;

	printf("%s: Testing message callbacks\n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(cs_config, (ctx, CS_SET, CS_MESSAGE_CB, (CS_VOID*) cslibmsg_cb, CS_UNUSED, NULL));

	/* set different callback for the connection */
	check_call(ct_callback, (NULL, conn, CS_SET, CS_CLIENTMSG_CB, (CS_VOID*) clientmsg_cb2));

	/* call all tests */
#undef TEST
#define TEST(name) test_ ## name();
	ALL_TESTS

	if (verbose) {
		printf("Trying logout\n");
	}
	check_call(try_ctlogout, (ctx, conn, cmd, verbose));

	if (verbose) {
		printf("Test succeeded\n");
	}
	return 0;
}

static void
_check_fail(const char *name, CS_RETCODE ret, int line)
{
	if (ret != CS_FAIL) {
		fprintf(stderr, "%s():%d: succeeded\n", name, line);
		exit(1);
	}
}
#define check_fail(func, args) do { \
	ct_reset_last_message(); \
	_check_fail(#func, func args, __LINE__); \
} while(0)

static void
_check_last_message(ct_message_type type, CS_INT number, const char *msg, int line)
{
	bool type_ok = true, number_ok = true, msg_ok = true;

	if (type == CTMSG_NONE && ct_last_message.type == type)
		return;

	type_ok = (ct_last_message.type == type);
	number_ok = (ct_last_message.number == number);
	if (msg && msg[0])
		msg_ok = (strstr(ct_last_message.text, msg) != NULL);
	if (!type_ok || !number_ok || !msg_ok)
		report_wrong_error(line);
}
#define check_last_message(type, number, msg) \
	_check_last_message(type, number, msg, __LINE__)

static void
test_ct_callback(void)
{
	void *ptr;

	/* this should fail, context or connection should be not NULL */
	check_fail(ct_callback, (NULL, NULL, CS_SET, CS_SERVERMSG_CB, servermsg_cb));
	check_last_message(CTMSG_NONE, 0, NULL);

	/* this should fail, context and connection cannot be both not NULL */
	check_fail(ct_callback, (ctx, conn, CS_SET, CS_SERVERMSG_CB, servermsg_cb));
	check_last_message(CTMSG_CLIENT2, 0x01010133, NULL);

	/* this should fail, invalid action */
	check_fail(ct_callback, (ctx, NULL, 3, CS_SERVERMSG_CB, servermsg_cb));
	check_last_message(CTMSG_CLIENT, 0x01010105, "action");

	/* this should fail, invalid action */
	check_fail(ct_callback, (NULL, conn, 3, CS_SERVERMSG_CB, servermsg_cb));
	check_last_message(CTMSG_CLIENT2, 0x01010105, "action");

	/* this should fail, invalid type */
	check_fail(ct_callback, (ctx, NULL, CS_SET, 20, servermsg_cb));
	check_last_message(CTMSG_CLIENT, 0x01010105, "type");

	/* this should fail, invalid type */
	check_fail(ct_callback, (NULL, conn, CS_SET, 20, servermsg_cb));
	check_last_message(CTMSG_CLIENT2, 0x01010105, "type");

	/* NULL func getting it */
	check_fail(ct_callback, (NULL, conn, CS_GET, CS_CLIENTMSG_CB, NULL));
	check_last_message(CTMSG_CLIENT2, 0x01010103, "The parameter func cannot be NULL");

	check_fail(ct_callback, (NULL, conn, CS_GET, CS_SERVERMSG_CB, NULL));
	check_last_message(CTMSG_CLIENT2, 0x01010103, "The parameter func cannot be NULL");

	/* invalid type with action CS_GET */
	check_fail(ct_callback, (NULL, conn, CS_GET, 20, NULL));
	check_last_message(CTMSG_CLIENT2, 0x01010105, "An illegal value of 20 given for parameter type");

	ptr = (char*)0 + 123;
	check_fail(ct_callback, (NULL, conn, CS_GET, 20, &ptr));
	check_last_message(CTMSG_CLIENT2, 0x01010105, "An illegal value of 20 given for parameter type");
	if (ptr != (char*)0 + 123) {
		fprintf(stderr, "Invalid pointer %p\n", ptr);
		exit(1);
	}
}

static void
test_ct_res_info(void)
{
	CS_RETCODE ret;
	CS_INT result_type;
	CS_INT num_cols;
	CS_INT count;

	check_call(ct_command, (cmd, CS_LANG_CMD, "SELECT 'hi' AS greeting", CS_NULLTERM, CS_UNUSED));
	check_call(ct_send, (cmd));

	while ((ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch (result_type) {
		case CS_CMD_SUCCEED:
		case CS_CMD_DONE:
			break;
		case CS_ROW_RESULT:
			/* this should fail, invalid number */
			check_fail(ct_res_info, (cmd, 1234, &num_cols, CS_UNUSED, NULL));
			check_last_message(CTMSG_CLIENT2, 0x01010105, "operation");

			while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED)
				continue;

			if (ret != CS_END_DATA) {
				fprintf(stderr, "ct_fetch() unexpected return %d.\n", (int) ret);
				exit(1);
			}
			break;
		default:
			fprintf(stderr, "ct_results() unexpected result_type %d.\n", (int) result_type);
			exit(1);
		}
	}
	if (ret != CS_END_RESULTS) {
		fprintf(stderr, "ct_results() unexpected return %d.\n", (int) ret);
		exit(1);
	}
}

static void
test_ct_send(void)
{
	/* reset command to idle state */
	check_call(ct_cmd_drop, (cmd));
	check_call(ct_cmd_alloc, (conn, &cmd));

	/* this should fail, invalid command state */
	check_fail(ct_send, (cmd));
	check_last_message(CTMSG_CLIENT2, 0x0101019b, "idle");
}

static void
test_cs_config(void)
{
	/* a set of invalid, not accepted values */
	static const CS_INT invalid_values[] = {
		-1,
		-5,
		-200,
		CS_WILDCARD,
		CS_NO_LIMIT,
		CS_UNUSED,
		0 /* terminator */
	};
	const CS_INT *p_invalid;
	CS_INT out_len;
	char out_buf[8];

	check_call(cs_config, (ctx, CS_SET, CS_USERDATA, (CS_VOID *)"test",  CS_NULLTERM, NULL));

	/* check that value written does not have the NUL terminator */
	strcpy(out_buf, "123456");
	check_call(cs_config, (ctx, CS_GET, CS_USERDATA, (CS_VOID *)out_buf, 8, NULL));
	if (strcmp(out_buf, "test56") != 0) {
		fprintf(stderr, "Wrong output buffer '%s'\n", out_buf);
		exit(1);
	}

	check_call(cs_config, (ctx, CS_SET, CS_USERDATA, (CS_VOID *)"test123",  4, NULL));

	/* check that value written does not have more characters */
	strcpy(out_buf, "123456");
	check_call(cs_config, (ctx, CS_GET, CS_USERDATA, (CS_VOID *)out_buf, 8, NULL));
	if (strcmp(out_buf, "test56") != 0) {
		fprintf(stderr, "Wrong output buffer '%s'\n", out_buf);
		exit(1);
	}

	for (p_invalid = invalid_values; *p_invalid != 0; ++p_invalid) {
		check_fail(cs_config, (ctx, CS_SET, CS_USERDATA, (CS_VOID *)"test", *p_invalid, NULL));
		check_last_message(CTMSG_CSLIB, 0x02010106, "buflen");
	}

	/* wrong action */
	check_fail(cs_config, (ctx, 1000, CS_USERDATA, (CS_VOID *)"test", 4, NULL));
	check_last_message(CTMSG_CSLIB, 0x02010106, "action");

	/* wrong property */
	check_fail(cs_config, (ctx, CS_SET, 100000, NULL, CS_UNUSED, NULL));
	check_last_message(CTMSG_CSLIB, 0x02010106, "property");

	/* read exactly expected bytes */
	check_call(cs_config, (ctx, CS_GET, CS_USERDATA, (CS_VOID *)out_buf, 4, NULL));

	/* wrong buflen getting value */
	out_len = -123;
	check_fail(cs_config, (ctx, CS_GET, CS_USERDATA, (CS_VOID *)out_buf, CS_NULLTERM, &out_len));
	check_last_message(CTMSG_CSLIB, 0x02010106, "buflen");
	if (out_len != -123) {
		fprintf(stderr, "Wrong buffer length returned\n");
		exit(1);
	}

	/* shorter buffer */
	out_len = -123;
	strcpy(out_buf, "123456");
	check_fail(cs_config, (ctx, CS_GET, CS_USERDATA, (CS_VOID *)out_buf, 2, &out_len));
	check_last_message(CTMSG_CSLIB, 0x02010102, " 2 bytes");
	if (out_len != 4) {
		fprintf(stderr, "Wrong buffer length returned\n");
		exit(1);
	}
	if (strcmp(out_buf, "123456") != 0) {
		fprintf(stderr, "Wrong buffer returned\n");
		exit(1);
	}
}

static void
test_blk_init(void)
{
	CS_BLKDESC *blkdesc;
        check_call(blk_alloc, (conn, BLK_VERSION_100, &blkdesc));

	/* invalid direction */
	check_fail(blk_init, (blkdesc, 100, "testname", CS_NULLTERM));
	check_last_message(CTMSG_CLIENT2, 0x0101010f, "CS_BLK_IN or CS_BLK_OUT");

	/* invalid tablename */
	check_fail(blk_init, (blkdesc, CS_BLK_IN, NULL, CS_NULLTERM));
	check_last_message(CTMSG_CLIENT2, 0x01010106, "tblname cannot be NULL");

	/* invalid tablename length */
	check_fail(blk_init, (blkdesc, CS_BLK_IN, "testname", -4));
	check_last_message(CTMSG_CLIENT2, 0x01010104, "tblnamelen has an illegal value of -4");

        check_call(blk_drop, (blkdesc));
}

static void
test_cs_loc_alloc(void)
{
	/* no context or locale */
	check_fail(cs_loc_alloc, (NULL, NULL));
	check_last_message(CTMSG_NONE, 0, NULL);

	/* no locale */
	check_fail(cs_loc_alloc, (ctx, NULL));
	check_last_message(CTMSG_CSLIB, 0x02010104, " loc_pointer cannot be NULL");
}

static void
test_cs_loc_drop(void)
{
	/* no context or locale */
	check_fail(cs_loc_drop, (NULL, NULL));
	check_last_message(CTMSG_NONE, 0, NULL);

	/* no locale */
	check_fail(cs_loc_drop, (ctx, NULL));
	check_last_message(CTMSG_CSLIB, 0x02010104, " locale cannot be NULL");
}

static void
test_cs_locale(void)
{
	CS_LOCALE *locale;

	check_call(cs_loc_alloc, (ctx, &locale));

	check_call(cs_locale, (ctx, CS_SET, locale, CS_SYB_CHARSET, "utf8", 4, NULL));

	/* no context  */
	check_fail(cs_locale, (NULL, CS_SET, locale, CS_SYB_CHARSET, "utf8", 4, NULL));
	check_last_message(CTMSG_NONE, 0, NULL);

	/* no locale  */
	check_fail(cs_locale, (ctx, CS_SET, NULL, CS_SYB_CHARSET, "utf8", 4, NULL));
	check_last_message(CTMSG_CSLIB, 0x02010104, " locale cannot be NULL");

	/* wrong action */
	check_fail(cs_locale, (ctx, 1000, locale, CS_SYB_CHARSET, "utf8", 4, NULL));
	check_last_message(CTMSG_CSLIB, 0x02010106, " 1000 was given for parameter action");

	/* wrong type */
	check_fail(cs_locale, (ctx, CS_SET, locale, 1000, "utf8", 4, NULL));
	check_last_message(CTMSG_CSLIB, 0x02010106, " 1000 was given for parameter type");

	/* wrong length  */
	check_fail(cs_locale, (ctx, CS_SET, locale, CS_SYB_CHARSET, "utf8", -4, NULL));
	check_last_message(CTMSG_CSLIB, 0x02010106, " -4 was given for parameter buflen");

	check_call(cs_loc_drop, (ctx, locale));
}

static void
test_ct_dynamic(void)
{
	/* wrong type */
	check_fail(ct_dynamic, (cmd, 10000, "test", CS_NULLTERM, "query", CS_NULLTERM));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " 10000 given for parameter type");

	/* wrong id length */
	check_fail(ct_dynamic, (cmd, CS_PREPARE, "test", -5, "query", CS_NULLTERM));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " -5 given for parameter idlen");

	/* wrong query length */
	check_fail(ct_dynamic, (cmd, CS_PREPARE, "test", CS_NULLTERM, "query", -6));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " -6 given for parameter buflen");

	/* wrong id and query length */
	check_fail(ct_dynamic, (cmd, CS_PREPARE, "test", -5, "query", -6));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " -5 given for parameter idlen");

	/* id not existing */
	check_fail(ct_dynamic, (cmd, CS_DEALLOC, "notexisting", CS_NULLTERM, NULL, CS_UNUSED));
	check_last_message(CTMSG_CLIENT2, 0x01010187, " specified id does not exist ");

	/* wrong id length */
	check_fail(ct_dynamic, (cmd, CS_DEALLOC, "notexisting", -7, NULL, CS_UNUSED));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " -7 given for parameter idlen");
}

static void
test_ct_connect(void)
{
	CS_CONNECTION *conn;

	check_call(ct_con_alloc, (ctx, &conn));

	/* wrong server name length */
	check_fail(ct_connect, (conn, "test", -5));
	check_last_message(CTMSG_CLIENT, 0x01010105, " -5 given for parameter snamelen");

	check_call(ct_con_drop, (conn));
}

static void
test_ct_command(void)
{
	/* wrong query length, CS_UNUSED, this was behaving differently */
	check_fail(ct_command, (cmd, CS_LANG_CMD, "test", CS_UNUSED, CS_UNUSED));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " given for parameter buflen");

	/* wrong query length */
	check_fail(ct_command, (cmd, CS_LANG_CMD, "test", -7, CS_UNUSED));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " -7 given for parameter buflen");

	/* wrong query length, RPC */
	check_fail(ct_command, (cmd, CS_RPC_CMD, "test", -3, CS_UNUSED));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " -3 given for parameter buflen");
}

static void
test_ct_cursor(void)
{
	/* wrong name length */
	check_fail(ct_cursor, (cmd, CS_CURSOR_DECLARE, "test", -4, "query", CS_NULLTERM, CS_UNUSED));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " -4 given for parameter namelen");

	/* wrong query length */
	check_fail(ct_cursor, (cmd, CS_CURSOR_DECLARE, "test", CS_NULLTERM, "query", -3, CS_UNUSED));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " -3 given for parameter tlen");

	/* wrong name and query length */
	check_fail(ct_cursor, (cmd, CS_CURSOR_DECLARE, "test", -11, "query", -3, CS_UNUSED));
	check_last_message(CTMSG_CLIENT2, 0x01010105, " -11 given for parameter namelen");
}

static void
test_ct_con_props(void)
{
	CS_CONNECTION *conn;

	check_call(ct_con_alloc, (ctx, &conn));

	/* wrong buffer length */
	check_fail(ct_con_props, (conn, CS_SET, CS_APPNAME, "app", -3, NULL));
	check_last_message(CTMSG_CLIENT, 0x01010105, " -3 given for parameter buflen");

	/* wrong buffer length, CS_UNUSED, it had a different behaviour */
	check_fail(ct_con_props, (conn, CS_SET, CS_APPNAME, "app", CS_UNUSED, NULL));
	check_last_message(CTMSG_CLIENT, 0x01010105, " given for parameter buflen");

	check_call(ct_con_drop, (conn));

	check_fail(ct_con_props, (NULL, CS_SET, CS_APPNAME, "app", 3, NULL));
	check_last_message(CTMSG_NONE, 0, NULL);
}

static void
test_cs_convert(void)
{
	CS_INT retlen;
	char outbuf[32];

	CS_DATAFMT destfmt, srcfmt;
	memset(&srcfmt, 0, sizeof(srcfmt));
	memset(&destfmt, 0, sizeof(destfmt));

	/* destdata == NULL */
	check_fail(cs_convert, (ctx, &srcfmt, "src", &destfmt, NULL, &retlen));
	check_last_message(CTMSG_CSLIB, 0x2010104, "The parameter destdata cannot be NULL");

	/* destfmt == NULL */
	check_fail(cs_convert, (ctx, &srcfmt, "src", NULL, outbuf, &retlen));
	check_last_message(CTMSG_CSLIB, 0x2010104, "The parameter destfmt cannot be NULL");

	/* destdata == NULL && destfmt == NULL */
	check_fail(cs_convert, (ctx, &srcfmt, "src", NULL, NULL, &retlen));
	check_last_message(CTMSG_CSLIB, 0x2010104, "The parameter destfmt cannot be NULL");

	/* invalid source type */
	srcfmt.datatype = 1234;
	check_fail(cs_convert, (ctx, &srcfmt, "src", &destfmt, outbuf, &retlen));
	check_last_message(CTMSG_CSLIB, 0x2010110, "Conversion between 1234 and 0 datatypes is not supported");
	srcfmt.datatype = 0;

	/* invalid destination type */
	destfmt.datatype = 1234;
	check_fail(cs_convert, (ctx, &srcfmt, "src", &destfmt, outbuf, &retlen));
	check_last_message(CTMSG_CSLIB, 0x2010110, "Conversion between 0 and 1234 datatypes is not supported");
	destfmt.datatype = 0;

	/* not fixed and maxlength == 0 */
	check_call(cs_convert, (ctx, &srcfmt, "src", &destfmt, outbuf, &retlen));

	/* not fixed and maxlength <= 0 */
	destfmt.maxlength = -1;
	check_fail(cs_convert, (ctx, &srcfmt, "src", &destfmt, outbuf, &retlen));
	check_last_message(CTMSG_CSLIB, 0x2010112,
			   "An illegal value of -1 was placed in the maxlength field of the CS_DATAFMT structure");
	destfmt.maxlength = 0;
}
