/** \file
 *  \brief Provide poll call where missing
 */

#if !defined(_tdsguard_g3Yr0q7NdWY6GI4uTB9PNx_) && !defined(HAVE_POLL)
#define _tdsguard_g3Yr0q7NdWY6GI4uTB9PNx_

#include <freetds/pushvis.h>

#ifndef _WIN32
/* poll flags */
# define POLLIN  0x0001
# define POLLOUT 0x0004
# define POLLERR 0x0008

/* synonyms */
# define POLLNORM POLLIN
# define POLLPRI POLLIN
# define POLLRDNORM POLLIN
# define POLLRDBAND POLLIN
# define POLLWRNORM POLLOUT
# define POLLWRBAND POLLOUT

/* ignored */
# define POLLHUP 0x0010
# define POLLNVAL 0x0020
typedef struct pollfd {
    int fd;		/* file descriptor to poll */
    short events;	/* events of interest on fd */
    short revents;	/* events that occurred on fd */
} pollfd_t;

#else /* Windows */
/*
 * Windows use different constants than Unix
 * Newer version have a WSAPoll which is equal to Unix poll
 */
# if !defined(POLLRDNORM) && !defined(POLLWRNORM)
#  define POLLIN  0x0300
#  define POLLOUT 0x0010
#  define POLLERR 0x0001
#  define POLLHUP 0x0002
#  define POLLNVAL 0x0004
#  define POLLRDNORM 0x0100
#  define POLLWRNORM 0x0010
typedef struct pollfd {
    SOCKET fd;	/* file descriptor to poll */
    short events;	/* events of interest on fd */
    short revents;	/* events that occurred on fd */
} pollfd_t;
# else
typedef struct pollfd pollfd_t;
# endif
#endif

#undef poll
int tds_poll(struct pollfd fds[], size_t nfds, int timeout);
#define poll(fds, nfds, timeout) tds_poll(fds, nfds, timeout)

#include <freetds/popvis.h>

#endif
