#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include "terminal.h"

static struct termios term;
static struct termios oterm;
static int term_init = 0;

int
save_term()
{
  int r;

  if (!isatty(fileno(stdin)))
  {
    return 1;
  }
  if (term_init)
  {
    return 0;
  }
  if ((r = tcgetattr(fileno(stdin), &oterm)) != 0)
  {
    return r;
  }
  term_init = 1;
  return 0;
}

int
set_term_noecho()
{
  int r;

  if (!isatty(fileno(stdin)))
  {
    return 1;
  }
  if ((r = save_term()) != 0)
  {
    return r;
  }
  if ((r = tcgetattr(fileno(stdin), &term)) != 0)
  {
    return r;
  }
  term.c_lflag &= ~(ICANON | ECHO);
  if ((r = tcsetattr(fileno(stdin), TCSANOW, &term)) != 0)
  {
    return r;
  }
  return 0;
}

int
reset_term()
{
  int r;

  if (!isatty(fileno(stdin)))
  {
    return 1;
  }
  if ((r = save_term()) != 0)
  {
    return r;
  }
  if ((r = tcsetattr(fileno(stdin), TCSANOW, &oterm)) != 0)
  {
    return r;
  }
  return 0;
}

