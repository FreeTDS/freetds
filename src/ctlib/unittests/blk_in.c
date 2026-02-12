#include "common.h"

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

#include <bkpublic.h>

#include <freetds/replacements.h>

static void
do_bind(CS_BLKDESC * blkdesc, int colnum, CS_INT host_format, CS_INT host_type, CS_INT host_maxlen,
	void *var_addr, CS_INT * var_len_addr, CS_SMALLINT * var_ind_addr);
static void do_one_bind(CS_BLKDESC * blkdesc, int col, const char *name);
static void do_type_bind(CS_BLKDESC * blkdesc, int col, const char *tok);
static void free_dynamic_bounded(void);
static void sendrow(CS_BLKDESC * blkdesc, const char *tok);
static FILE *open_test_file(const char *filename);
typedef enum
{
	PART_END,
	PART_SQL,
	PART_BIND,
	PART_OUTPUT,
} part_t;
static part_t read_part_type(FILE * in);
static char *read_part(FILE * in);
static void read_line(char *buf, size_t buf_len, FILE * f);
static char *append_string(char *s1, const char *s2);
static char *get_output(CS_COMMAND * cmd, bool distinct);
static void single_test(CS_CONNECTION * conn, CS_COMMAND * cmd, FILE * in);

/*
 * Static data for insertion
 */
static int  not_null_bit = 1;
static CS_INT      l_not_null_bit = 4;
static CS_SMALLINT i_not_null_bit = 0;

static char not_null_char[] = "a char";
static CS_INT      l_not_null_char = 6;
static CS_SMALLINT i_not_null_char = 0;

static char not_null_varchar[] = "a varchar";
static CS_INT      l_not_null_varchar = 9;
static CS_SMALLINT i_not_null_varchar = 0;

static char not_null_datetime[] = "Dec 17 2003  3:44PM";
static CS_INT      l_not_null_datetime = 19;
static CS_SMALLINT i_not_null_datetime = 0;

static char not_null_smalldatetime[] = "Dec 17 2003  3:44PM";
static CS_INT      l_not_null_smalldatetime = 19;
static CS_SMALLINT i_not_null_smalldatetime = 0;

static char not_null_money[] = "12.34";
static CS_INT      l_not_null_money = 5;
static CS_SMALLINT i_not_null_money = 0;

static char not_null_smallmoney[] = "12.34";
static CS_INT      l_not_null_smallmoney = 5;
static CS_SMALLINT i_not_null_smallmoney = 0;

static char not_null_float[] = "12.34";
static CS_INT      l_not_null_float = 5;
static CS_SMALLINT i_not_null_float = 0;

static char not_null_real[] = "12.34";
static CS_INT      l_not_null_real = 5;
static CS_SMALLINT i_not_null_real = 0;

static char not_null_decimal[] = "12.34";
static CS_INT      l_not_null_decimal = 5;
static CS_SMALLINT i_not_null_decimal = 0;

static char not_null_numeric[] = "12.34";
static CS_INT      l_not_null_numeric = 5;
static CS_SMALLINT i_not_null_numeric = 0;

static int  not_null_int        = 1234;
static CS_INT      l_not_null_int = 4;
static CS_SMALLINT i_not_null_int = 0;

static int  not_null_smallint   = 1234;
static CS_INT      l_not_null_smallint = 4;
static CS_SMALLINT i_not_null_smallint = 0;

static int  not_null_tinyint    = 123;
static CS_INT      l_not_null_tinyint = 4;
static CS_SMALLINT i_not_null_tinyint = 0;

static CS_INT      l_null_char = 0;
static CS_SMALLINT i_null_char = -1;

static CS_INT      l_null_varchar = 0;
static CS_SMALLINT i_null_varchar = -1;

static CS_INT      l_null_datetime = 0;
static CS_SMALLINT i_null_datetime = -1;

static CS_INT      l_null_smalldatetime = 0;
static CS_SMALLINT i_null_smalldatetime = -1;

static CS_INT      l_null_money = 0;
static CS_SMALLINT i_null_money = -1;

static CS_INT      l_null_smallmoney = 0;
static CS_SMALLINT i_null_smallmoney = -1;

static CS_INT      l_null_float = 0;
static CS_SMALLINT i_null_float = -1;

static CS_INT      l_null_real = 0;
static CS_SMALLINT i_null_real = -1;

static CS_INT      l_null_decimal = 0;
static CS_SMALLINT i_null_decimal = -1;

static CS_INT      l_null_numeric = 0;
static CS_SMALLINT i_null_numeric = -1;

static CS_INT      l_null_int = 0;
static CS_SMALLINT i_null_int = -1;

static CS_INT      l_null_smallint = 0;
static CS_SMALLINT i_null_smallint = -1;

static CS_INT      l_null_tinyint = 0;
static CS_SMALLINT i_null_tinyint = -1;

static char not_null_varbinary[] = "123456789";
static CS_INT      l_not_null_varbinary = 9;
static CS_SMALLINT i_not_null_varbinary = 0;

static void
do_binds(CS_BLKDESC *blkdesc, FILE *in, bool *rows_sent)
{
	char line[1024];
	int col = 1;

	*rows_sent = false;
	for (;;) {
		const char *tok;

		read_line(line, sizeof(line), in);
		if (strcmp(line, "--\n") == 0)
			return;
		tok = strtok(line, " \t\n");

		if (strcmp(tok, "--") == 0) {
			/* comment */
			continue;
		} else if (strncmp(tok, "SENDROW", 7) == 0) {
			sendrow(blkdesc, tok);
			col = 0;
			*rows_sent = true;
		} else if (strncmp(tok, "CS_", 3) == 0) {
			do_type_bind(blkdesc, col, tok);
		} else {
			do_one_bind(blkdesc, col, line);
		}
		++col;
	}
}

static void
do_one_bind(CS_BLKDESC *blkdesc, int col, const char *name)
{
#define do_bind(bind_name, fmt, type, len, value) do { \
	if (strcmp(#bind_name, name) == 0) { \
		do_bind(blkdesc, col, fmt, type, len, value, &l_ ## bind_name, &i_ ## bind_name); \
		return; \
	} \
} while(0)

	/* non nulls */

	do_bind(not_null_bit, CS_FMT_UNUSED, CS_INT_TYPE, 4, &not_null_bit);
	do_bind(not_null_char, CS_FMT_NULLTERM, CS_CHAR_TYPE, 7, not_null_char);
	do_bind(not_null_varchar, CS_FMT_NULLTERM, CS_CHAR_TYPE, 10, not_null_varchar);
	do_bind(not_null_datetime, CS_FMT_NULLTERM, CS_CHAR_TYPE, 20, not_null_datetime);
	do_bind(not_null_smalldatetime, CS_FMT_NULLTERM, CS_CHAR_TYPE, 20, not_null_smalldatetime);
	do_bind(not_null_money, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_money);
	do_bind(not_null_smallmoney, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_smallmoney);
	do_bind(not_null_float, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_float);
	do_bind(not_null_real, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_real);
	do_bind(not_null_decimal, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_decimal);
	do_bind(not_null_numeric, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_numeric);
	do_bind(not_null_int, CS_FMT_UNUSED, CS_INT_TYPE, 4, &not_null_int);
	do_bind(not_null_smallint, CS_FMT_UNUSED, CS_INT_TYPE, 4, &not_null_smallint);
	do_bind(not_null_tinyint, CS_FMT_UNUSED, CS_INT_TYPE, 4, &not_null_tinyint);
	do_bind(not_null_varbinary, CS_FMT_NULLTERM, CS_BINARY_TYPE, 10, not_null_varbinary);

	/* nulls */

	do_bind(null_char, CS_FMT_NULLTERM, CS_CHAR_TYPE, 7, not_null_char);
	do_bind(null_varchar, CS_FMT_NULLTERM, CS_CHAR_TYPE, 10, not_null_varchar);
	do_bind(null_datetime, CS_FMT_NULLTERM, CS_CHAR_TYPE, 20, not_null_datetime);
	do_bind(null_smalldatetime, CS_FMT_NULLTERM, CS_CHAR_TYPE, 20, not_null_smalldatetime);
	do_bind(null_money, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_money);
	do_bind(null_smallmoney, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_smallmoney);
	do_bind(null_float, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_float);
	do_bind(null_real, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_real);
	do_bind(null_decimal, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_decimal);
	do_bind(null_numeric, CS_FMT_NULLTERM, CS_CHAR_TYPE, 6, not_null_numeric);
	do_bind(null_int, CS_FMT_UNUSED, CS_INT_TYPE, 4, &not_null_int);
	do_bind(null_smallint, CS_FMT_UNUSED, CS_INT_TYPE, 4, &not_null_smallint);
	do_bind(null_tinyint, CS_FMT_UNUSED, CS_INT_TYPE, 4, &not_null_tinyint);
#undef do_bind

	fprintf(stderr, "Column %s not found\n", name);
	exit(1);
}

typedef struct bound_value
{
	struct bound_value *next;
	CS_INT varlen;
	CS_SMALLINT ind;
	void *value;
} bound_value;

static bound_value *dynamic_bounded = NULL;

static void
free_dynamic_bounded(void)
{
	while (dynamic_bounded) {
		bound_value *next = dynamic_bounded->next;

		free(dynamic_bounded->value);
		free(dynamic_bounded);
		dynamic_bounded = next;
	}
}

static void
do_type_bind(CS_BLKDESC *blkdesc, int col, const char *tok)
{
	if (strcmp(tok, "CS_CHAR_TYPE") == 0) {
		CS_INT maxlen = atoi(strtok(NULL, " \t\n"));
		char *value = strdup(strtok(NULL, "\n"));
		bound_value *bound = calloc(1, sizeof(*bound));

		assert(value);
		assert(bound);

		if (strncmp(value, "\"\"\"", 3) == 0) {
			char *p;

			memmove(value, value + 3, strlen(value + 2));
			p = strstr(value, "\"\"\"");
			if (p)
				*p = '\0';
		}
		bound->varlen = strlen(value);
		bound->ind = 0;
		bound->value = value;
		bound->next = dynamic_bounded;
		dynamic_bounded = bound;
		do_bind(blkdesc, col, CS_FMT_NULLTERM, CS_CHAR_TYPE, maxlen, value, &bound->varlen, &bound->ind);

		return;
	}
	fprintf(stderr, "Unhandled type %s\n", tok);
	exit(1);
}

static void
sendrow(CS_BLKDESC *blkdesc, const char *tok)
{
	const char *errtype_str;
	ct_message_type errtype;
	CS_INT number;
	const char *message;

	ct_reset_last_message();
	if (strcmp(tok, "SENDROWERROR") == 0)
		check_fail(blk_rowxfer, (blkdesc));
	else
		check_call(blk_rowxfer, (blkdesc));

	errtype_str = strtok(NULL, " \t\n");
	if (!errtype_str) {
		check_last_message(CTMSG_NONE, 0, NULL);
		return;
	}

	if (strcmp(errtype_str, "CLIENT") == 0)
		errtype = CTMSG_CLIENT;
	else if (strcmp(errtype_str, "CLIENT2") == 0)
		errtype = CTMSG_CLIENT2;
	else if (strcmp(errtype_str, "SERVER") == 0)
		errtype = CTMSG_SERVER;
	else if (strcmp(errtype_str, "CSLIB") == 0)
		errtype = CTMSG_CSLIB;
	else {
		fprintf(stderr, "Unknown error type %s\n", errtype_str);
		exit(1);
	}

	number = strtol(strtok(NULL, " \t\n"), NULL, 0);
	message = strtok(NULL, "\n");
	check_last_message(errtype, number, message);
}

static void
do_bind(CS_BLKDESC *blkdesc, int colnum, CS_INT host_format, CS_INT host_type, CS_INT host_maxlen,
	void *var_addr, CS_INT *var_len_addr, CS_SMALLINT *var_ind_addr)
{
	CS_DATAFMT datafmt;

	check_call(blk_describe, (blkdesc, colnum, &datafmt));

	datafmt.format = host_format;
	datafmt.datatype = host_type;
	datafmt.maxlength = host_maxlen;
	datafmt.count = 1;

	check_call(blk_bind, (blkdesc, colnum, &datafmt, var_addr, var_len_addr, var_ind_addr ));
}

static const char table_name[] = "all_types_bcp_unittest";

TEST_MAIN()
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	bool verbose = false;
	FILE *in;
	part_t part;

	printf("%s: Retrieve data using array binding \n", __FILE__);
	if (verbose) {
		printf("Trying login\n");
	}
	in = open_test_file(argc > 1 ? argv[1] : NULL);
	check_call(try_ctlogin, (&ctx, &conn, &cmd, verbose));

	check_call(cs_config, (ctx, CS_SET, CS_MESSAGE_CB, (CS_VOID *) cslibmsg_cb, CS_UNUSED, NULL));
	check_call(ct_callback, (NULL, conn, CS_SET, CS_CLIENTMSG_CB, (CS_VOID *) clientmsg_cb2));

	for (;;) {
		part = read_part_type(in);
		if (part == PART_END)
			break;
		assert(part == PART_SQL);

		single_test(conn, cmd, in);
	}

	printf("done\n");

	check_call(try_ctlogout, (ctx, conn, cmd, verbose));
	fclose(in);

	return 0;
}

static void
single_test(CS_CONNECTION *conn, CS_COMMAND *cmd, FILE *in)
{
	char command[512];
	char *create_table_sql, *out1, *out2;
	CS_BLKDESC *blkdesc;
	int count = 0;
	int i;
	part_t part;
	CS_RETCODE ret;
	bool rows_sent = false;

	sprintf(command, "if exists (select 1 from sysobjects where type = 'U' and name = '%s') drop table %s",
		table_name, table_name);

	check_call(run_command, (cmd, command));

	create_table_sql = read_part(in);
	ret = run_command(cmd, create_table_sql);
	free(create_table_sql);

	/* on error skip the test */
	if (ret != CS_SUCCEED) {
		part = read_part_type(in);
		assert(part == PART_BIND);
		free(read_part(in));

		part = read_part_type(in);
		assert(part == PART_OUTPUT);
		free(read_part(in));
		return;
	}

	sprintf(command, "delete from %s", table_name);
	check_call(run_command, (cmd, command));

	check_call(blk_alloc, (conn, BLK_VERSION_150, &blkdesc));

	check_call(blk_init, (blkdesc, CS_BLK_IN, (char *) table_name, CS_NULLTERM));

	part = read_part_type(in);
	assert(part == PART_BIND);

	do_binds(blkdesc, in, &rows_sent);

	part = read_part_type(in);
	assert(part == PART_OUTPUT);

	if (!rows_sent) {
		printf("Sending same row 10 times... \n");
		for (i = 0; i < 10; i++)
			check_call(blk_rowxfer, (blkdesc));
	} else {
		check_call(blk_rowxfer, (blkdesc));
	}

	check_call(blk_done, (blkdesc, CS_BLK_ALL, &count));

	blk_drop(blkdesc);

	free_dynamic_bounded();

	printf("%d rows copied.\n", count);

	out1 = read_part(in);
	out2 = get_output(cmd, !rows_sent);
	if (strcmp(out1, out2) != 0) {
		fprintf(stderr, "Wrong output\n-- expected --\n%s\n-- got --\n%s\n--\n", out1, out2);
		exit(1);
	}
	free(out1);
	free(out2);
}

static char *
get_output(CS_COMMAND *cmd, bool distinct)
{
	char command[512];

	CS_RETCODE ret;
	CS_RETCODE results_ret;
	CS_DATAFMT datafmt;
	CS_INT datalength;
	CS_SMALLINT *inds = NULL;
	CS_INT count, row_count = 0;
	CS_INT result_type;
	CS_CHAR *data = NULL;
	CS_INT num_cols;
	CS_INT i;
	char *out = strdup("");

	assert(out != NULL);

	sprintf(command, "select%s * from %s", distinct ? " distinct" : "", table_name);
	check_call(ct_command, (cmd, CS_LANG_CMD, command, CS_NULLTERM, CS_UNUSED));

	check_call(ct_send, (cmd));
	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		switch ((int) result_type) {
		case CS_CMD_SUCCEED:
			break;
		case CS_CMD_DONE:
			break;
		case CS_CMD_FAIL:
			fprintf(stderr, "ct_results() result_type CS_CMD_FAIL.\n");
			exit(1);
		case CS_ROW_RESULT:
			check_call(ct_res_info, (cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL));
			data = malloc(num_cols * 256);
			assert(data != NULL);
			inds = calloc(num_cols, sizeof(*inds));
			assert(inds != NULL);
			for (i = 0; i < num_cols; i++) {
				datafmt.datatype = CS_CHAR_TYPE;
				datafmt.format = CS_FMT_NULLTERM;
				datafmt.maxlength = 256;
				datafmt.count = 1;
				datafmt.locale = NULL;
				check_call(ct_bind, (cmd, i + 1, &datafmt, data + 256 * i, &datalength, &inds[i]));
			}

			while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count)) == CS_SUCCEED) {
				row_count += count;
				for (i = 0; i < num_cols; i++) {
					if (!inds[i])
						out = append_string(out, data + 256 * i);
					else
						out = append_string(out, "NULL");
					out = append_string(out, "\n");
				}
			}
			switch ((int) ret) {
			case CS_END_DATA:
				break;
			case CS_FAIL:
				fprintf(stderr, "ct_fetch() returned CS_FAIL.\n");
				exit(1);
			case CS_ROW_FAIL:
				fprintf(stderr, "ct_fetch() CS_ROW_FAIL on row %d.\n", row_count);
				exit(1);
			default:
				fprintf(stderr, "ct_fetch() unexpected return.\n");
				exit(1);
			}
			break;
		case CS_COMPUTE_RESULT:
			fprintf(stderr, "ct_results() unexpected CS_COMPUTE_RESULT.\n");
			exit(1);
		default:
			fprintf(stderr, "ct_results() unexpected result_type.\n");
			exit(1);
		}
	}
	switch ((int) results_ret) {
	case CS_END_RESULTS:
		break;
	case CS_FAIL:
		fprintf(stderr, "ct_results() failed.\n");
		exit(1);
		break;
	default:
		fprintf(stderr, "ct_results() unexpected return.\n");
		exit(1);
	}

	free(data);
	free(inds);
	return out;
}


static FILE *
open_test_file(const char *filename)
{
	FILE *input_file = NULL;
	char in_file[256];

	/* If no filename requested, try blk_in.in in both the expected location and the current directory. */
	if (filename)
		input_file = fopen(filename, "r");
	else {
		snprintf(in_file, sizeof(in_file), "%s/blk_in.in", FREETDS_SRCDIR);
		filename = in_file;
		input_file = fopen(filename, "r");
		if (!input_file) {
			filename = "blk_in.in";
			input_file = fopen(filename, "r");
		}
	}
	if (!input_file) {
		fprintf(stderr, "could not open %s\n", filename);
		exit(1);
	}
	return input_file;
}

static part_t
read_part_type(FILE *in)
{
	char line[1024];
	part_t part;

	read_line(line, sizeof(line), in);
	if (strncmp(line, "end", 3) == 0)
		return PART_END;

	if (strcmp(line, "sql\n") == 0)
		part = PART_SQL;
	else if (strcmp(line, "bind\n") == 0)
		part = PART_BIND;
	else if (strcmp(line, "output\n") == 0)
		part = PART_OUTPUT;
	else {
		fprintf(stderr, "Invalid part: %s\n", line);
		exit(1);
	}
	read_line(line, sizeof(line), in);
	if (strcmp(line, "--\n") != 0) {
		fprintf(stderr, "Error reading line\n");
		exit(1);
	}
	return part;
}

static char *
read_part(FILE *in)
{
	char line[1024];
	char *part = strdup("");

	assert(part != NULL);
	for (;;) {
		read_line(line, sizeof(line), in);
		if (strcmp(line, "--\n") == 0)
			return part;
		part = append_string(part, line);
	}
}

static void
read_line(char *buf, size_t buf_len, FILE *f)
{
	if (fgets(buf, buf_len, f) == NULL || ferror(f)) {
		fprintf(stderr, "Error reading line\n");
		exit(1);
	}
}

static char *
append_string(char *s1, const char *s2)
{
	assert(s1);
	assert(s2);
	s1 = realloc(s1, strlen(s1) + strlen(s2) + 1);
	assert(s1 != NULL);
	strcat(s1, s2);
	return s1;
}
