#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sybfront.h>
#include <sybdb.h>
#include "edit.h"
#include "terminal.h"

int
edit(const char *editor, const char *arg)
{
  int pid;

 retry_fork:
  pid = fork();
  switch(pid) {
  case -1:
    if (errno == EAGAIN)
    {
      sleep(5);
      goto retry_fork;
    }
    perror("fisql");
    reset_term();
    dbexit();
    exit(EXIT_FAILURE);
    break;
  case 0:
    execlp(editor, editor, arg, (char *) 0);
    fprintf(stderr, "Unable to invoke the '%s' editor.\n", editor);
    exit(EXIT_FAILURE);
    break;
  default:
    waitpid(pid, (int *) 0, 0);
    break;
  }
  return 0;
}

