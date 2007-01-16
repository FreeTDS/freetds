/*	$Id: interrupt.h,v 1.1 2007-01-16 21:33:12 castellano Exp $	*/
extern sigjmp_buf restart;

void inactive_interrupt_handler(int sig);
void active_interrupt_handler(int sig);
void maybe_handle_active_interrupt(void);
int active_interrupt_pending(DBPROCESS *dbproc);
int active_interrupt_servhandler(DBPROCESS *dbproc);



