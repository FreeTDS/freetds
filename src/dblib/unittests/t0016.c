/* 
 * Purpose: Test bcp in and out, and specifically bcp_colfmt()
 * Functions: bcp_colfmt bcp_columns bcp_exec bcp_init 
 */

#include "common.h"

static int failed = 0;

static void
failure(const char *fmt, ...)
{
	va_list ap;

	failed = 1;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
}

#define INFILE_NAME "t0016"
#define TABLE_NAME "#dblib0016"

static void test_file(const char *fn);
static int compare_files(const char *fn1, const char *fn2);
static unsigned count_file_rows(FILE *f);
static DBPROCESS *dbproc;

int
main(int argc, char *argv[])
{
	LOGINREC *login;
	char in_file[30];
	unsigned int n;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	set_malloc_options();

	read_login_info(argc, argv);
	printf("Starting %s\n", argv[0]);
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	BCP_SETL(login, TRUE);
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0016");
	DBSETLCHARSET(login, "UTF-8");

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

static int got_error = 0;

static int
ignore_msg_handler(DBPROCESS * dbproc, DBINT msgno, int state, int severity, char *text, char *server, char *proc, int line)
{
	got_error = 1;
	return 0;
}

static int
ignore_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
	got_error = 1;
	return INT_CANCEL;
}

static char line1[1024*16];
static char line2[1024*16];

static void
test_file(const char *fn)
{
	int i;
	RETCODE ret;
	int num_cols = 0;
	const char *out_file = "t0016.out";
	const char *err_file = "t0016.err";
	DBINT rows_copied;
	unsigned num_rows = 2;

	FILE *input_file;

	char in_file[256];
	snprintf(in_file, sizeof(in_file), "%s/%s.in", FREETDS_SRCDIR, fn);

	input_file = fopen(in_file, "rb");
	if (!input_file) {
		sprintf(in_file, "%s.in", fn);
		input_file = fopen(in_file, "rb");
	}
	if (!input_file) {
		fprintf(stderr, "could not open %s\n", in_file);
		exit(1);
	}
	num_rows = count_file_rows(input_file);
	fclose(input_file);

	dberrhandle(ignore_err_handler);
	dbmsghandle(ignore_msg_handler);

	printf("Creating table '%s'\n", TABLE_NAME);
	got_error = 0;
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS)
		continue;

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	if (got_error)
		return;

	/* BCP in */

	printf("bcp_init with in_file as '%s'\n", in_file);
	ret = bcp_init(dbproc, TABLE_NAME, in_file, err_file, DB_IN);
	if (ret != SUCCEED)
		failure("bcp_init failed\n");

	printf("return from bcp_init = %d\n", ret);

	ret = sql_cmd(dbproc);
	printf("return from dbcmd = %d\n", ret);

	ret = dbsqlexec(dbproc);
	printf("return from dbsqlexec = %d\n", ret);

	if (dbresults(dbproc) != FAIL) {
		num_cols = dbnumcols(dbproc);
		printf("Number of columns = %d\n", num_cols);

		while (dbnextrow(dbproc) != NO_MORE_ROWS) {
		}
	}

	ret = bcp_columns(dbproc, num_cols);
	if (ret != SUCCEED)
		failure("bcp_columns failed\n");
	printf("return from bcp_columns = %d\n", ret);

	for (i = 1; i < num_cols; i++) {
		if ((ret = bcp_colfmt(dbproc, i, SYBCHAR, 0, -1, (const BYTE *) "\t", sizeof(char), i)) == FAIL)
			failure("return from bcp_colfmt = %d\n", ret);
	}

	if ((ret = bcp_colfmt(dbproc, num_cols, SYBCHAR, 0, -1, (const BYTE *) "\n", sizeof(char), num_cols)) == FAIL)
		failure("return from bcp_colfmt = %d\n", ret);


	ret = bcp_exec(dbproc, &rows_copied);
	if (ret != SUCCEED || rows_copied != num_rows)
		failure("bcp_exec failed\n");

	printf("%d rows copied in\n", rows_copied);

	/* BCP out */

	rows_copied = 0;
	ret = bcp_init(dbproc, TABLE_NAME, out_file, err_file, DB_OUT);
	if (ret != SUCCEED)
		failure("bcp_int failed\n");

	printf("select\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != FAIL) {
		num_cols = dbnumcols(dbproc);
		while (dbnextrow(dbproc) != NO_MORE_ROWS) {
		}
	}

	ret = bcp_columns(dbproc, num_cols);

	for (i = 1; i < num_cols; i++) {
		if ((ret = bcp_colfmt(dbproc, i, SYBCHAR, 0, -1, (const BYTE *) "\t", sizeof(char), i)) == FAIL)
			failure("return from bcp_colfmt = %d\n", ret);
	}

	if ((ret = bcp_colfmt(dbproc, num_cols, SYBCHAR, 0, -1, (const BYTE *) "\n", sizeof(char), num_cols)) == FAIL)
		failure("return from bcp_colfmt = %d\n", ret);

	ret = bcp_exec(dbproc, &rows_copied);
	if (ret != SUCCEED || rows_copied != num_rows)
		failure("bcp_exec failed\n");

	printf("%d rows copied out\n", rows_copied);

	printf("Dropping table '%s'\n", TABLE_NAME);
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS)
		continue;

	if (failed)
		return;

	if (compare_files(in_file, out_file))
		printf("Input and output files are equal\n");
	else
		failed = 1;
}

static size_t fgets_raw(char *s, int len, FILE *f)
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

static int compare_files(const char *fn1, const char *fn2)
{
	int equal = 1;
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
				equal = 0;
				failure("error reading a file or EOF of a file\n");
				break;
			}

			/* EOF or error of both */
			if (!s1) {
				if (feof(f1) && feof(f2))
					break;
				equal = 0;
				failure("error reading a file\n");
				break;
			}

			if (s1 != s2 || memcmp(line1, line2, s1) != 0) {
				equal = 0;
				failure("File different at line %d\n"
					" input: %s"
					" output: %s",
					line, line1, line2);
			}
		}
	} else {
		equal = 0;
		failure("error opening files\n");
	}
	if (f1)
		fclose(f1);
	if (f2)
		fclose(f2);

	return equal;
}

static unsigned count_file_rows(FILE *f)
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
