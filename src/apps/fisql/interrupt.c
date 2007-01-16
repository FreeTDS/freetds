#include <setjmp.h>
#include <signal.h>
#include <sybfront.h>
#include <sybdb.h>
#include "interrupt.h"

sigjmp_buf restart;
volatile int os_interrupt = 0;

void
inactive_interrupt_handler(int sig)
{
  siglongjmp(restart, 1);
}

void
active_interrupt_handler(int sig)
{
  os_interrupt = sig;
}

void
maybe_handle_active_interrupt(void)
{
  int sig;

  if (os_interrupt)
  {
    sig = os_interrupt;
    os_interrupt = 0;
    inactive_interrupt_handler(sig);
  }
}

int
active_interrupt_pending(DBPROCESS *dbproc)
{
  if (os_interrupt)
  {
    return TRUE;
  }
  return FALSE;
}

int
active_interrupt_servhandler(DBPROCESS *dbproc)
{
  return INT_CANCEL;
}
