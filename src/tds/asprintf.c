/*
 * vasprintf(3), asprintf(3)
 * 20020809 entropy@tappedin.com
 * public domain.  no warranty.  use at your own risk.  have a nice day.
 *
 * Assumes that mprotect(2) granularity is one page.  May need adjustment
 * on systems with unusual protection semantics.
 */

#if !HAVE_VASPRINTF

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/mman.h>
#include <setjmp.h>
#include <signal.h>
#include <assert.h>

static char  software_version[]   = "$Id: asprintf.c,v 1.1 2002-08-22 03:27:51 jklowden Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};
static jmp_buf env;

static void
sigsegv(int sig)
{
  longjmp(env, 1);
}

int
vasprintf(char **ret, const char *fmt, va_list ap)
{
  volatile char *buf = NULL;
  volatile unsigned int pgs;
  struct sigaction sa, osa;
  int len;
  long pgsize = sysconf(_SC_PAGE_SIZE);

  pgs = ((strlen(fmt) + 1) / pgsize) + 1;
  if (sigaction(SIGSEGV, NULL, &osa)) {
    *ret = NULL;
    return -1;
  }
  sa.sa_handler = sigsegv;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (setjmp(env) != 0) {
    mprotect((void *) (buf + pgs * pgsize), pgsize, PROT_READ|PROT_WRITE);
    free((void *) buf);
    pgs++;
  }
  if ((buf = valloc((pgs + 1) * pgsize)) == NULL) {
    *ret = NULL;
    return -1;
  }
  assert(((unsigned long) buf % pgsize) == 0);
  if (sigaction(SIGSEGV, &sa, NULL)) {
    free((void *) buf);
    *ret = NULL;
    return -1;
  }
  mprotect((void *) (buf + pgs * pgsize), pgsize, PROT_NONE);
  len = vsprintf((void *) buf, fmt, ap);
  mprotect((void *) (buf + pgs * pgsize), pgsize, PROT_READ|PROT_WRITE);
  if (sigaction(SIGSEGV, &osa, NULL)) {
    free((void *) buf);
    *ret = NULL;
    return -1;
  }
  if (len < 0) {
    free((void *) buf);
    *ret = NULL;
    return len;
  }
  if ((buf = realloc((void *) buf, len + 1)) == NULL) {
    *ret = NULL;
    return -1;
  }
  *ret = (char *) buf;
  return len;
}

int
asprintf(char **ret, const char *fmt, ...)
{
  int len;
  va_list ap;

  va_start(ap, fmt);
  len = vasprintf(ret, fmt, ap);
  va_end(ap);
  return len;
}
#endif /* HAVE_VASPRINTF */
