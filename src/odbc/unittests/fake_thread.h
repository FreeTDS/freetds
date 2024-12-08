/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <freetds/thread.h>

#include <freetds/pushvis.h>

extern tds_thread fake_thread;

/* function to be provided to use init_fake_server() */
TDS_THREAD_PROC_DECLARE(fake_thread_proc, arg);

/**
 * Initialize fake server and thread with specified TCP port.
 * The server will listen on local address.
 */
bool init_fake_server(int ip_port);

#include <freetds/popvis.h>
