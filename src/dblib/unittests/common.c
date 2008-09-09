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

#ifndef DBNTWIN32
#include "replacements.h"
#endif

static char software_version[] = "$Id: common.c,v 1.24 2008-09-09 14:48:03 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

typedef struct _tag_memcheck_t
{
	int item_number;
	unsigned int special;
	struct _tag_memcheck_t *next;
}
memcheck_t;


static memcheck_t *breadcrumbs = NULL;
static int num_breadcrumbs = 0;
static const unsigned int BREADCRUMB = 0xABCD7890;

char USER[512];
char SERVER[512];
char PASSWORD[512];
char DATABASE[512];
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
	*p = 0;
	return path;
}
#define dirname tds_dirname

#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 512
#endif

int
read_login_info(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	
	FILE *in = NULL;
#if !defined(__MINGW32__) && !defined(_MSC_VER)
	int ch;
#endif
	char line[512];
	char *s1, *s2;
	char filename[MAXPATHLEN];
	static const char *PWD = "../../../PWD";
	struct { char *username, *password, *servername, *database; char fverbose; } options;
	
	BASENAME = tds_basename((char *)argv[0]);
	DIRNAME = dirname((char *)argv[0]);
	
	memset(&options, 0, sizeof(options));
	
#if !defined(__MINGW32__) && !defined(_MSC_VER)
	/* process command line options (handy for manual testing) */
	while ((ch = getopt(argc, (char**)argv, "U:P:S:D:f:v")) != -1) {
		switch (ch) {
		case 'U':
			options.username = strdup(optarg);
			break;
		case 'P':
			options.password = strdup(optarg);
			break;
		case 'S':
			options.servername = strdup(optarg);
			break;
		case 'D':
			options.database = strdup(optarg);
			break;
		case 'f': /* override default PWD file */
			PWD = strdup(optarg);
			break;
		case 'v':
			options.fverbose = 1; /* doesn't normally do anything */
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
#endif
	strcpy(filename, PWD);

	s1 = getenv("TDSPWDFILE");
	if (s1 && s1[0])
		in = fopen(s1, "r");
	if (!in)
		in = fopen(filename, "r");
	if (!in)
		in = fopen("PWD", "r");
	if (!in) {
		sprintf(filename, "%s/%s", (DIRNAME) ? DIRNAME : ".", PWD);

		in = fopen(filename, "r");
		if (!in) {
			fprintf(stderr, "Can not open %s file\n\n", filename);
			return 1;
		}
	}

	while (fgets(line, 512, in)) {
		s1 = strtok(line, "=");
		s2 = strtok(NULL, "\n");
		if (!s1 || !s2)
			continue;
		if (!strcmp(s1, "UID")) {
			strcpy(USER, s2);
		} else if (!strcmp(s1, "SRV")) {
			strcpy(SERVER, s2);
		} else if (!strcmp(s1, "PWD")) {
			strcpy(PASSWORD, s2);
		} else if (!strcmp(s1, "DB")) {
			strcpy(DATABASE, s2);
		}
	}
	fclose(in);
	
	/* apply command-line overrides */
	if (options.username) {
		strcpy(USER, options.username);
		free(options.username);
	}
	if (options.password) {
		strcpy(PASSWORD, options.password);
		free(options.password);
	}
	if (options.servername) {
		strcpy(SERVER, options.servername);
		free(options.servername);
	}
	if (options.database) {
		strcpy(DATABASE, options.database);
		free(options.database);
	}
	
	printf("found %s.%s for %s in \"%s\"\n", SERVER, DATABASE, USER, filename);
	return 0;
}

void
check_crumbs(void)
{
	int i;
	memcheck_t *ptr = breadcrumbs;

	i = num_breadcrumbs;
	while (ptr != NULL) {
		if (ptr->special != BREADCRUMB || ptr->item_number != i) {
			fprintf(stderr, "Somebody overwrote one of the bread crumbs!!!\n");
			abort();
		}

		i--;
		ptr = ptr->next;
	}
}



void
add_bread_crumb(void)
{
	memcheck_t *tmp;

	check_crumbs();

	tmp = (memcheck_t *) calloc(sizeof(memcheck_t), 1);
	if (tmp == NULL) {
		fprintf(stderr, "Out of memory");
		abort();
		exit(1);
	}

	num_breadcrumbs++;

	tmp->item_number = num_breadcrumbs;
	tmp->special = BREADCRUMB;
	tmp->next = breadcrumbs;

	breadcrumbs = tmp;
}

void
free_bread_crumb(void)
{
	memcheck_t *tmp, *ptr = breadcrumbs;

	check_crumbs();

	while (ptr) {
		tmp = ptr->next;
		free(ptr);
		ptr = tmp;
	}
	num_breadcrumbs = 0;
}

int
syb_msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line)
{
	char var_value[31];
	int i;
	char *c;
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

		if (msgtext != NULL && (c = strchr(msgtext, '\'')) != NULL) {
			i = 0;
			for (++c; i <= 30 && *c != '\0' && *c != '\''; ++c)
				var_value[i++] = *c;
			var_value[i] = '\0';
		}
		return 0;
	}

	/*
	 * If the user data indicates this is an expected error message (because we're testing the 
	 * error propogation, say) then indicate this message was anticipated.
	 */
	if (dbproc != NULL) {
		pexpected_msgno = (int *) dbgetuserdata(dbproc);
		if (pexpected_msgno && *pexpected_msgno == msgno) {
			fprintf(stdout, "OK: anticipated message arrived: %d %s\n", msgno, msgtext);
			*pexpected_msgno = 0;
			return 0;
		}
	}
	/*
	 * If the severity is something other than 0 or the msg number is
	 * 0 (user informational messages).
	 */
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
			fprintf(stdout, "%s\n", msgtext);
			fflush(stdout);
		}
	}

	assert(0); /* no unanticipated messages allowed in unit tests */
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
			fprintf(stdout, "OK: anticipated error %d (%s) arrived\n", dberr, dberrstr);
			*pexpected_dberr = 0;
			return INT_CANCEL;
		}
	}

	fprintf(stderr,
		"DB-LIBRARY error (severity %d, dberr %d, oserr %d, dberrstr %s, oserrstr %s):\n",
		severity, dberr, oserr, dberrstr ? dberrstr : "(null)", oserrstr ? oserrstr : "(null)");
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

	assert(0); /* no unanticipated errors allowed in unit tests */
	return INT_CANCEL;
}
