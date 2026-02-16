/* 
 * Purpose: Test bcp in and out, and specifically bcp_colfmt()
 * Functions: bcp_colfmt bcp_columns bcp_exec bcp_init 
 */

#include "common.h"

#include <freetds/bool.h>
#include <freetds/replacements.h>

static bool failed = false;

static void
failure(const char *fmt, ...)
{
	va_list ap;

	failed = true;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#define INFILE_NAME "t0016"
#define TABLE_NAME "#dblib0016"

static void test_file(const char *fn);
static bool compare_files(const char *fn1, const char *fn2);
static unsigned count_file_rows(FILE *f);
static size_t fgets_raw(char *s, int len, FILE * f);
static DBPROCESS *dbproc;

TEST_MAIN()
{
	LOGINREC *login;
	char in_file[30];
	unsigned int n;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	set_malloc_options();

	read_login_info(argc, argv);
	printf("Starting %s\n", argv[0]);
	dbsetversion(DBVERSION_100);
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	BCP_SETL(login, TRUE);
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0016");
	DBSETLCHARSET(login, "utf8");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE)) {
		dbuse(dbproc, DATABASE);
	}
	dbloginfree(login);
	printf("After logon\n");

	strcpy(in_file, INFILE_NAME);
	for (n = 1; n <= 100; ++n) {
		test_file(in_file);
		sprintf(in_file, "%s_%d", INFILE_NAME, n);
		if (sql_reopen(in_file) != SUCCEED)
			break;
	}

	dbclose(dbproc);
	dbexit();

	printf("dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	return failed ? 1 : 0;
}

static bool got_error = false;

static int
ignore_msg_handler(DBPROCESS * dbproc TDS_UNUSED, DBINT msgno TDS_UNUSED, int state TDS_UNUSED, int severity TDS_UNUSED,
		   char *text TDS_UNUSED, char *server TDS_UNUSED, char *proc TDS_UNUSED, int line TDS_UNUSED)
{
	got_error = true;
	return 0;
}

static int
ignore_err_handler(DBPROCESS * dbproc TDS_UNUSED, int severity TDS_UNUSED, int dberr TDS_UNUSED,
		   int oserr TDS_UNUSED, char *dberrstr TDS_UNUSED, char *oserrstr TDS_UNUSED)
{
	got_error = true;
	return INT_CANCEL;
}

/* T0016 data files are all tab-delimited char columns, but we support omitting
 * columns in order to simulate testing of BCP with a format file that omits
 * columns (feature supported by ASE, but not MSSQL)
 */
static void
format_columns(int table_cols, char* col_list)
{
	char const* seps = ", ";
	int colnum = 1;	/* bcp_colfmt uses 1-based indexing */
	RETCODE ret;
	char const* tok = NULL;
	int num_cols;

	/* Count number of columns wanted */
	if (col_list && col_list[0])
	{
		printf("Custom columns: %s\n", col_list);
		num_cols = 0;
		char* dup = strdup(col_list);
		for (tok = strtok(dup, seps); tok; tok = strtok(NULL, seps))
			++num_cols;
		free(dup);

		tok = strtok(col_list, seps);
	}
	else
		num_cols = table_cols;

	if ( num_cols == 0 )
	{ 
		failure("PARAM cols contained no valid items.\n");
		return;
	}

	ret = bcp_columns(dbproc, num_cols);
	if ( ret != SUCCEED )
	{
		failure("return from bcp_columns = %d\n", ret);
		return;
	}

	for (colnum = 1; colnum <= num_cols; ++colnum)
	{
		int host_column;

		if (tok)
		{
			host_column = atoi(tok);
			if (host_column < 1 || host_column > table_cols)
				failure("PARAM cols %d out of range (1-%d)\n", host_column, table_cols);
			tok = strtok(NULL, seps);
		}
		else
			host_column = colnum;

		ret = bcp_colfmt(dbproc, host_column, SYBCHAR, 0, -1,
			(BYTE*)(colnum == num_cols ? "\n" : "\t"), sizeof(char), colnum);

		if (ret == FAIL)
			failure("return from bcp_colfmt(%d,%d) = FAIL\n", host_column, colnum);
	}
}
static char line1[1024*16];
static char line2[1024*16];

static void
test_file(const char *fn)
{
	RETCODE ret;
	int num_cols = 0;
	const char *out_file = "t0016.out";
	const char *err_file = "t0016.err";
	DBINT rows_copied;
	unsigned num_rows = 2;
	char table_name[64];
	char hints[64];
	char colin[64] = { 0 };
	char colout[64] = { 0 };

	FILE *input_file;

	char sql_file[256];
	char in_file[256];
	char exp_file[256];

	snprintf(sql_file, sizeof(sql_file), "%s/%s.sql", FREETDS_SRCDIR, fn);
	snprintf(in_file, sizeof(in_file), "%s/%s.in", FREETDS_SRCDIR, fn);
	snprintf(exp_file, sizeof(exp_file), "%s/%s.exp", FREETDS_SRCDIR, fn);

	strlcpy(table_name, TABLE_NAME, sizeof(table_name));
	hints[0] = 0;

	input_file = fopen(in_file, "r");
	if (!input_file) {
		sprintf(in_file, "%s.in", fn);
		sprintf(exp_file, "%s.exp", fn);
		input_file = fopen(in_file, "rb");
	}
	if (!input_file) {
		fprintf(stderr, "could not open %s\n", in_file);
		exit(1);
	}
	num_rows = count_file_rows(input_file);
	fclose(input_file);

	input_file = fopen(exp_file, "r");
	if (!input_file)
		strcpy(exp_file, in_file);
	else
		fclose(input_file);

	input_file = fopen(sql_file, "r");
	assert(input_file);
	for (;;) {
		const char *param;
		size_t len = fgets_raw(line1, sizeof(line1), input_file);

		if (len && line1[len - 1] == '\n')
			line1[len - 1] = '\0';
		if (len == 0 || strncmp(line1, "-- PARAM:", 9) != 0)
			break;
		param = line1 + 9;
		if (strncmp(param, "table ", 6) == 0) {
			param += 6;
			strlcpy(table_name, param, sizeof(table_name));
		} else if (strncmp(param, "hints ", 6) == 0) {
			param += 6;
			strlcpy(hints, param, sizeof(hints));
		} else if (strncmp(param, "colin ", 6) == 0) {
			param += 6;
			strlcpy(colin, param, sizeof(colin));
		} else if (strncmp(param, "colout ", 7) == 0) {
			param += 7;
			strlcpy(colout, param, sizeof(colout));
		} else {
			fprintf(stderr, "invalid parameter: %s\n", param);
			exit(1);
		}
	}
	fclose(input_file);

	dberrhandle(ignore_err_handler);
	dbmsghandle(ignore_msg_handler);

	printf("Creating table '%s'\n", table_name);
	got_error = false;
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS)
		continue;

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	if (got_error)
		return;

	ret = sql_cmd(dbproc);
	printf("return from dbcmd = %d\n", ret);

	ret = dbsqlexec(dbproc);
	printf("return from dbsqlexec = %d\n", ret);

	if (dbresults(dbproc) != FAIL) {
		num_cols = dbnumcols(dbproc);
		printf("Number of columns = %d\n", num_cols);

		while (dbnextrow(dbproc) != NO_MORE_ROWS)
			continue;
	}

	/* BCP in */
	printf("bcp_init with in_file as '%s'\n", in_file);
	ret = bcp_init(dbproc, table_name, in_file, (char*) err_file, DB_IN);
	if (ret != SUCCEED)
		failure("bcp_init failed\n");

	if (hints[0])
		bcp_options(dbproc, BCPHINTS, (BYTE *) hints, strlen(hints));

	printf("return from bcp_init = %d\n", ret);

	format_columns(num_cols, colin);

	ret = bcp_exec(dbproc, &rows_copied);
	if (ret != SUCCEED || rows_copied != num_rows)
		failure("bcp_exec failed\n");

	printf("%d rows copied in\n", rows_copied);

	/* BCP out */

	rows_copied = 0;
	ret = bcp_init(dbproc, table_name, (char *) out_file, (char *) err_file, DB_OUT);
	if (ret != SUCCEED)
		failure("bcp_init failed\n");

	printf("select\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != FAIL) {
		num_cols = dbnumcols(dbproc);
		while (dbnextrow(dbproc) != NO_MORE_ROWS)
			continue;
	}

	format_columns(num_cols, colout);

	ret = bcp_exec(dbproc, &rows_copied);
	if (ret != SUCCEED || rows_copied != num_rows)
		failure("bcp_exec failed\n");

	printf("%d rows copied out\n", rows_copied);

	printf("Dropping table '%s'\n", table_name);
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS)
		continue;

	if (failed)
		return;

	if (compare_files(exp_file, out_file))
		printf("Input and output files are equal\n");
	else
		failed = true;
}

static size_t
fgets_raw(char *s, int len, FILE *f)
{
	char *p = s;

	while (len > 1) {
		int c = getc(f);
		if (c == EOF) {
			if (ferror(f))
				return 0;
			break;
		}
		*p++ = c;
		--len;
		if (c == '\n')
			break;
	}
	if (len > 0)
		*p = 0;
	return p - s;
}

static bool
compare_files(const char *fn1, const char *fn2)
{
	bool equal = true;
	FILE *f1, *f2;
	size_t s1, s2;

	/* check input and output should be the same */
	f1 = fopen(fn1, "r");
	f2 = fopen(fn2, "r");
	if (f1 != NULL && f2 != NULL) {
		int line = 1;

		for (;; ++line) {
			s1 = fgets_raw(line1, sizeof(line1), f1);
			s2 = fgets_raw(line2, sizeof(line2), f2);

			/* EOF or error of one */
			if (!!s1 != !!s2) {
				equal = false;
				failure("error reading a file or EOF of a file\n");
				break;
			}

			/* EOF or error of both */
			if (!s1) {
				if (feof(f1) && feof(f2))
					break;
				equal = false;
				failure("error reading a file\n");
				break;
			}

			if (s1 != s2 || memcmp(line1, line2, s1) != 0) {
				equal = false;
				failure("File different at line %d\n"
					"  input: %s"
					" output: %s",
					line, line1, line2);
			}
		}
	} else {
		equal = false;
		failure("error opening files\n");
	}
	if (f1)
		fclose(f1);
	if (f2)
		fclose(f2);

	return equal;
}

static unsigned
count_file_rows(FILE *f)
{
	size_t s;
	unsigned rows = 1;
	char last = '\n';

	assert(f);

	while ((s = fgets_raw(line1, sizeof(line1), f)) != 0) {
		last = line1[s-1];
		if (last == '\n')
			++rows;
	}
	if (last == '\n')
		--rows;
	assert(!ferror(f));
	return rows;
}
