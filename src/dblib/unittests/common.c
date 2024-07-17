#include "common.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_LIBGEN_H
#include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */

#include <freetds/time.h>
#include <freetds/bool.h>

#if HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif /* HAVE_SYS_RESOURCE_H */

#include <freetds/replacements.h>

#if !defined(PATH_MAX)
#define PATH_MAX 256
#endif

static char sql_file[PATH_MAX];
static FILE* input_file = NULL;

static char *ARGV0 = NULL;
static char *ARGV0B = NULL;
static char *DIRNAME = NULL;
static const char *BASENAME = NULL;

#if HAVE_MALLOC_OPTIONS
extern const char *malloc_options;
#endif /* HAVE_MALLOC_OPTIONS */

void
set_malloc_options(void)
{

#if HAVE_MALLOC_OPTIONS
	/*
	 * Options for malloc
	 * A- all warnings are fatal
	 * J- init memory to 0xD0
	 * R- always move memory block on a realloc
	 */
	malloc_options = "AJR";
#endif /* HAVE_MALLOC_OPTIONS */
}

#if defined(__MINGW32__) || defined(_MSC_VER)
static char *
tds_dirname(char* path)
{
	char *p, *p2;

	for (p = path + strlen(path); --p > path && (*p == '/' || *p == '\\');)
		*p = '\0';

	p = strrchr(path, '/');
	if (!p)
		p = path;
	p2 = strrchr(p, '\\');
	if (p2)
		p = p2;
	if (p == path) {
		if (*p == '/' || *p == '\\')
			return "\\";
		return ".";
	}
	*p = 0;
	return path;
}
#define dirname tds_dirname

#endif

static bool free_file_registered = false;
static void
free_file(void)
{
	if (input_file) {
		fclose(input_file);
		input_file = NULL;
	}
	if (ARGV0) {
		DIRNAME = NULL;
		BASENAME = NULL;
		free(ARGV0);
		ARGV0 = NULL;
		free(ARGV0B);
		ARGV0B = NULL;
	}
}

int
read_login_info(int argc, char **argv)
{
	int len;
	int ch;
	char *s1 TDS_UNUSED;
	char filename[PATH_MAX];
	static const char *PWD = DEFAULT_PWD_PATH;
	const char *final_filename = NULL;

#if defined(HAVE_SETRLIMIT) && defined(RLIMIT_STACK)
#define MAX_STACK (8*1024*1024)

	struct rlimit rlim;

	if (!getrlimit(RLIMIT_STACK, &rlim) && (rlim.rlim_cur == RLIM_INFINITY || rlim.rlim_cur > MAX_STACK)) {
		rlim.rlim_cur = MAX_STACK;
		setrlimit(RLIMIT_STACK, &rlim);
	}
#endif

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	free(ARGV0);
	free(ARGV0B);
#ifdef __VMS
	{
		/* basename expects unix format */
		s1 = strrchr(argv[0], ';'); /* trim version; extension trimmed later */
		if (s1) *s1 = 0;
		const char *unixspec = decc$translate_vms(argv[0]);
		ARGV0 = strdup(unixspec);
	}
#else
	ARGV0 = strdup(argv[0]);
#endif
	ARGV0B = strdup(ARGV0);

	BASENAME = basename(ARGV0B);
#if defined(_WIN32) || defined(__VMS)
	s1 = strrchr(BASENAME, '.');
	if (s1) *s1 = 0;
#endif
	DIRNAME = dirname(ARGV0);

	/* process command line options (handy for manual testing) */
	while ((ch = getopt(argc, (char**)argv, "U:P:S:D:f:v")) != -1) {
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
		case 'v': /* doesn't normally do anything */
			common_pwd.fverbose = 1;
			break;
		case '?':
		default:
			fprintf(stderr, "usage:  %s \n"
					"        [-U username] [-P password]\n"
					"        [-S servername] [-D database]\n"
					"        [-i input filename] [-o output filename] "
					"[-e error filename]\n"
					, BASENAME);
			exit(1);
		}
	}
	strlcpy(filename, PWD, sizeof(filename));
	if (!(final_filename = try_read_login_info_base(&common_pwd, filename))
	    && !(final_filename = try_read_login_info_base(&common_pwd, "PWD"))) {
		sprintf(filename, "%s/%s", (DIRNAME) ? DIRNAME : ".", PWD);
		final_filename = read_login_info_base(&common_pwd, filename);
	}

	if (!*common_pwd.server) {
		fprintf(stderr, "no servername provided, quitting.\n");
		exit(1);
	}

	printf("found %s.%s for %s in \"%s\"\n", common_pwd.server, common_pwd.database, common_pwd.user, final_filename);

#if 0
	dbrecftos(BASENAME);
#endif
	len = snprintf(sql_file, sizeof(sql_file), "%s/%s.sql", FREETDS_SRCDIR, BASENAME);
	assert(len >= 0 && len <= sizeof(sql_file));

	if (input_file)
		fclose(input_file);
	if ((input_file = fopen(sql_file, "r")) == NULL) {
		fflush(stdout);
		fprintf(stderr, "could not open SQL input file \"%s\"\n", sql_file);
	}

	if (!free_file_registered)
		atexit(free_file);
	free_file_registered = true;

	printf("SQL text will be read from %s\n", sql_file);

	return 0;
}

/*
 * Fill the command buffer from a file while echoing it to standard output.
 */
RETCODE
sql_cmd(DBPROCESS *dbproc)
{
	char line[2048], *p = line;
	int i = 0;
	RETCODE erc=SUCCEED;

	if (!input_file) {
		fprintf(stderr, "%s: error: SQL input file \"%s\" not opened\n", BASENAME, sql_file);
		exit(1);
	}

	while ((p = fgets(line, (int)sizeof(line), input_file)) != NULL && strcasecmp("go\n", p) != 0) {
		printf("\t%3d: %s", ++i, p);
		if ((erc = dbcmd(dbproc, p)) != SUCCEED) {
			fprintf(stderr, "%s: error: could write \"%s\" to dbcmd()\n", BASENAME, p);
			exit(1);
		}
	}

	if (ferror(input_file)) {
		fprintf(stderr, "%s: error: could not read SQL input file \"%s\"\n", BASENAME, sql_file);
		exit(1);
	}

	return erc;
}

RETCODE
sql_rewind(void)
{
	if (!input_file)
		return FAIL;
	rewind(input_file);
	return SUCCEED;
}

RETCODE
sql_reopen(const char *fn)
{
	int len = snprintf(sql_file, sizeof(sql_file), "%s/%s.sql", FREETDS_SRCDIR, fn);
	assert(len >= 0 && len <= sizeof(sql_file));

	if (input_file)
		fclose(input_file);
	if ((input_file = fopen(sql_file, "r")) == NULL) {
		fflush(stdout);
		fprintf(stderr, "could not open SQL input file \"%s\"\n", sql_file);
		sql_file[0] = 0;
		return FAIL;
	}
	return SUCCEED;
}

int
syb_msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line)
{
	int *pexpected_msgno;

	/*
	 * Check for "database changed", or "language changed" messages from
	 * the client.  If we get one of these, then we need to pull the
	 * name of the database or charset from the message and set the
	 * appropriate variable.
	 */
	if (msgno == 5701 ||	/* database context change */
	    msgno == 5703 ||	/* language changed */
	    msgno == 5704) {	/* charset changed */

		/* fprintf( stderr, "msgno = %d: %s\n", msgno, msgtext ) ; */
		return 0;
	}

	/*
	 * If the user data indicates this is an expected error message (because we're testing the
	 * error propogation, say) then indicate this message was anticipated.
	 */
	if (dbproc != NULL) {
		pexpected_msgno = (int *) dbgetuserdata(dbproc);
		if (pexpected_msgno && *pexpected_msgno == msgno) {
			printf("OK: anticipated message arrived: %d %s\n", (int) msgno, msgtext);
			*pexpected_msgno = 0;
			return 0;
		}
	}
	/*
	 * If the severity is something other than 0 or the msg number is
	 * 0 (user informational messages).
	 */
	fflush(stdout);
	if (severity >= 0 || msgno == 0) {
		/*
		 * If the message was something other than informational, and
		 * the severity was greater than 0, then print information to
		 * stderr with a little pre-amble information.
		 */
		if (msgno > 0 && severity > 0) {
			fprintf(stderr, "Msg %d, Level %d, State %d\n", (int) msgno, (int) severity, (int) msgstate);
			fprintf(stderr, "Server '%s'", srvname);
			if (procname != NULL && *procname != '\0')
				fprintf(stderr, ", Procedure '%s'", procname);
			if (line > 0)
				fprintf(stderr, ", Line %d", line);
			fprintf(stderr, "\n");
			fprintf(stderr, "%s\n", msgtext);
			fflush(stderr);
		} else {
			/*
			 * Otherwise, it is just an informational (e.g. print) message
			 * from the server, so send it to stdout.
			 */
			printf("%s\n", msgtext);
			fflush(stdout);
			severity = 0;
		}
	}

	if (severity) {
		fprintf(stderr, "exit: no unanticipated messages allowed in unit tests\n");
		exit(EXIT_FAILURE);
	}
	return 0;
}

int
syb_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
	int *pexpected_dberr;

	/*
	 * For server messages, cancel the query and rely on the
	 * message handler to spew the appropriate error messages out.
	 */
	if (dberr == SYBESMSG)
		return INT_CANCEL;

	/*
	 * If the user data indicates this is an expected error message (because we're testing the
	 * error propogation, say) then indicate this message was anticipated.
	 */
	if (dbproc != NULL) {
		pexpected_dberr = (int *) dbgetuserdata(dbproc);
		if (pexpected_dberr && *pexpected_dberr == dberr) {
			printf("OK: anticipated error %d (%s) arrived\n", dberr, dberrstr);
			*pexpected_dberr = 0;
			return INT_CANCEL;
		}
	}

	fflush(stdout);
	fprintf(stderr,
		"DB-LIBRARY error (dberr %d (severity %d): \"%s\"; oserr %d: \"%s\")\n",
		dberr, severity, dberrstr ? dberrstr : "(null)", oserr, oserrstr ? oserrstr : "(null)");
	fflush(stderr);

	/*
	 * If the dbprocess is dead or the dbproc is a NULL pointer and
	 * we are not in the middle of logging in, then we need to exit.
	 * We can't do anything from here on out anyway.
	 * It's OK to end up here in response to a dbconvert() that
	 * resulted in overflow, so don't exit in that case.
	 */
	if ((dbproc == NULL) || DBDEAD(dbproc)) {
		if (dberr != SYBECOFL) {
			exit(255);
		}
	}

	if (severity) {
		fprintf(stderr, "error: no unanticipated errors allowed in unit tests\n");
		exit(EXIT_FAILURE);
	}

	return INT_CANCEL;
}
