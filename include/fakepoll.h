/* $Id: fakepoll.h,v 1.1 2009-01-07 02:58:32 jklowden Exp $ */
#if !defined(_FAKE_POLL_H) && !defined(HAVE_POLL)
#define _FAKE_POLL_H

#if HAVE_CONFIG_H
#include <config.h>
#endif 


#if HAVE_LIMITS_H
#include <limits.h>
#endif 

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif 

#if defined(WIN32)
#include <winsock2.h>
#endif

#if !defined(FD_SETSIZE)
# if !defined(OPEN_MAX)
# error cannot establish FD_SETSIZE
# endif
#define FD_SETSIZE OPEN_MAX
#endif

// poll flags
#define POLLIN  0x0001
#define POLLOUT 0x0004
#define POLLERR 0x0008

// synonyms
#define POLLNORM POLLIN
#define POLLPRI POLLIN
#define POLLRDNORM POLLIN
#define POLLRDBAND POLLIN
#define POLLWRNORM POLLOUT
#define POLLWRBAND POLLOUT

// ignored
#define POLLHUP 0x0010
#define POLLNVAL 0x0020

typedef struct pollfd {
    int fd;		/* file descriptor to poll */
    short events;	/* events of interest on fd */
    short revents;	/* events that occurred on fd */
} pollfd_t;


int fakepoll(struct pollfd fds[], int nfds, int timeout);

#endif
