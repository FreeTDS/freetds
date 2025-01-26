#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <getopt.h>
#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#else
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#define close(s) closesocket(s)
typedef int socklen_t;
typedef unsigned int in_addr_t;
#define sleep(s) Sleep((s)*1000)
#define poll WSAPoll
#endif
#include <gnutls/gnutls.h>

#include "dump_write.h"

/* This small application make man-in-the-middle with a crypted SQL Server
 * to be able to see decrypted login.
 * Works only with mssql2k or later.
 * Based on GnuTLS echo example.
 * compile with:
 *    gcc -O2 -Wall -o bounce bounce.c dump_write.c -lgnutls
 */

/* path to certificate, can be created with
 * openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout mycert.pem -out mycert.pem
 */
#define CERTFILE "mycert.pem"

/* address to connect to, the real server you want to tunnel */
static struct addrinfo *server_addrs = NULL;

#define SA struct sockaddr
#define SOCKET_ERR(err,s) if (err==-1) { perror(s); exit(1); }
#define MAX_BUF 4096
#define DH_BITS 1024

/* These are global */
static gnutls_certificate_credentials_t x509_cred;
static bool debug = false;
static tcpdump_writer* dump_writer = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int put_packet(int sd, unsigned char *const packet, int packet_len);
static int get_packet(int sd, unsigned char *const packet);
static void hexdump(const unsigned char *buffer, int len);
static void handle_session(int sd);
static void* handle_client_thread(void *);

static const unsigned char packet_type = 0x12;

static int log_recv(int sock, void *data, int len, int flags)
{
	int ret = recv(sock, data, len, flags);
	int save_errno = errno;
	if (ret > 0 && debug) {
		printf("got buffer from recv %d\n", sock);
		hexdump(data, ret);
	}
	errno = save_errno;
	return ret;
}
#undef recv
#define recv(a,b,c,d) log_recv(a,b,c,d)

static int log_send(int sock, const void *data, int len, int flags)
{
	int ret = send(sock, data, len, flags);
	int save_errno = errno;
	if (ret > 0 && debug) {
		printf("sent buffer with send %d\n", sock);
		hexdump(data, ret);
	}
	errno = save_errno;
	return ret;
}
#undef send
#define send(a,b,c,d) log_send(a,b,c,d)

typedef struct session {
	gnutls_session_t tls;
	int sock;
	int pos; // only pull
	int to_send;
	unsigned char packet[4096];
	int packet_len;
} session;

static ssize_t
tds_pull_func(gnutls_transport_ptr_t ptr, void *data, size_t len)
{
	session *const session = (struct session *) ptr;

	if (debug)
		fprintf(stderr, "in tds_pull_func\n");

	/* if we have some data send it */
	int packet_len = session->packet_len;
	if (session->to_send && packet_len >= 8) {
		session->packet[1] = 1;
		if (put_packet(session->sock, session->packet, packet_len) < 0)
			exit(1);
		session->packet_len = packet_len = 0;
		session->to_send = 0;
	}

	/* read from packet */
	int pos = session->pos;
	if (!packet_len || pos >= packet_len) {
		packet_len = get_packet(session->sock, session->packet);
		if (packet_len == 0)
			return 0;
		if (packet_len < 0)
			exit(1);
		session->packet_len = packet_len;
		pos = 8;
	}
	if (packet_len < 0)
		exit(1);
	if (len > (packet_len - pos))
		len = packet_len - pos;
	memcpy(data, session->packet + pos, len);
	session->pos = pos + len;
	if (debug)
		printf("read %d bytes\n", (int) len);
	return len;
}

static ssize_t
tds_push_func(gnutls_transport_ptr_t ptr, const void *data, size_t len)
{
	session *const session = (struct session *) ptr;
	int left;

	/* write to packet */
	if (!session->to_send)
		session->packet_len = 8;
	session->to_send = 1;
	session->packet[0] = packet_type;
	left = 4096 - session->packet_len;
	if (left <= 0) {
		session->packet[1] = 0;	/* not last */
		if (put_packet(session->sock, session->packet, session->packet_len) < 0)
			exit(1);
		session->packet_len = 8;
		left = 4096 - session->packet_len;
	}
	session->packet[1] = 1;		/* last */
	if (len > left)
		len = left;
	memcpy(session->packet + session->packet_len, data, len);
	session->packet_len += len;
	session->packet[2] = session->packet_len >> 8;
	session->packet[3] = session->packet_len;
	return len;
}

static ssize_t
tds_pull_null_func(gnutls_transport_ptr_t ptr, void *data, size_t len)
{
	int sock = (char*) ptr - (char*) 0;

	return recv(sock, data, len, 0);
}

static ssize_t
tds_push_null_func(gnutls_transport_ptr_t ptr, const void *data, size_t len)
{
	int sock = (char*) ptr - (char*) 0;

	return send(sock, data, len, 0);
}

static gnutls_session_t
initialize_tls_session(unsigned flags)
{
	gnutls_session_t session;

	if (gnutls_init(&session, flags) != GNUTLS_E_SUCCESS) {
		fprintf(stderr, "Error initializing TLS session\n");
		return NULL;
	}
	gnutls_transport_set_pull_function(session, tds_pull_func);
	gnutls_transport_set_push_function(session, tds_push_func);

	/* avoid calling all the priority functions, since the defaults
	 * are adequate.
	 */
	gnutls_set_default_priority(session);
	gnutls_priority_set_direct(session, "NORMAL:%COMPAT:-VERS-SSL3.0", NULL);

	/* mssql does not like padding too much */
	gnutls_record_disable_padding(session);

	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, x509_cred);

	gnutls_dh_set_prime_bits(session, DH_BITS);

	return session;
}

static gnutls_dh_params_t dh_params;

static int
generate_dh_params(void)
{

	/* Generate Diffie Hellman parameters - for use with DHE
	 * kx algorithms. These should be discarded and regenerated
	 * once a day, once a week or once a month. Depending on the
	 * security requirements.
	 */
	gnutls_dh_params_init(&dh_params);
	gnutls_dh_params_generate2(dh_params, DH_BITS);

	return 0;
}

static void
tds_tls_log(int level, const char *s)
{
	printf("(%d) %s", level, s);
}

static int
tcp_connect(void)
{
	int err, sd;

	/* connects to server
	 */
	sd = socket(server_addrs->ai_family, SOCK_STREAM, 0);
	if (sd < 0) {
		perror("socket");
		return -1;
	}

	err = connect(sd, server_addrs->ai_addr, server_addrs->ai_addrlen);
	if (err < 0) {
		close(sd);
		fprintf(stderr, "Connect error\n");
		return -1;
	}

	return sd;
}

/* Read plain TDS packet from socket */
static int
get_packet(int sd, unsigned char *const packet)
{
	if (debug)
		printf("get_packet\n");
	int packet_len = 0;
	for (;;) {
		int full_len = 4;
		if (packet_len >= 4) {
			full_len = packet[2] * 0x100 + packet[3];
			if (full_len < 8) {
				fprintf(stderr, "Reveived packet too small %d\n", full_len);
				return -1;
			}
			if (full_len > 4096) {
				fprintf(stderr, "Reveived packet too large %d\n", full_len);
				return -1;
			}
		}

		int l = recv(sd, (void *) (packet + packet_len), full_len - packet_len, 0);
		if (l <= 0) {
			fprintf(stderr, "error recv %s\n", strerror(errno));
			return l < 0 ? -1 : 0;
		}
		packet_len += l;

		if (full_len >= 8 && packet_len == full_len)
			break;
	}
	return packet_len;
}

/* Write plain TDS packet to socket */
static int
put_packet(int sd, unsigned char *const packet, const int packet_len)
{
	int sent = 0;

	if (debug)
		printf("put_packet\n");
	for (; sent < packet_len;) {
		int l = send(sd, (void *) (packet + sent), packet_len - sent, 0);

		if (l <= 0) {
			fprintf(stderr, "error send\n");
			return -1;
		}
		sent += l;
	}
	return sent;
}

/* Read encrypted TDS packet */
static int
get_packet_tls(gnutls_session_t tls, unsigned char *const packet, int full_packet_len)
{
	int packet_len = 0;

	if (debug)
		printf("get_packet_tls\n");
	for (;;) {
		int full_len = 4;
		if (packet_len >= 4) {
			full_len = packet[2] * 0x100 + packet[3];
			if (full_len < 8) {
				fprintf(stderr, "Reveived packet too small %d\n", full_len);
				return -1;
			}
			if (full_len > full_packet_len) {
				fprintf(stderr, "Reveived packet too large %d\n", full_len);
				return -1;
			}
		}

		const int l = gnutls_record_recv(tls, (void *) (packet + packet_len), full_len - packet_len);
		if (l == GNUTLS_E_INTERRUPTED)
			continue;
		if (l <= 0) {
			fprintf(stderr, "error recv\n");
			return -1;
		}
		packet_len += l;

		if (full_len >= 8 && packet_len == full_len)
			break;
	}
	return packet_len;
}

/* Write encrypted TDS packet */
static int
put_packet_tls(gnutls_session_t tls, unsigned char *packet, int packet_len)
{
	int sent = 0;

	if (debug)
		printf("put_packet_tls\n");
	for (; sent < packet_len;) {
		int l = gnutls_record_send(tls, (void *) (packet + sent), packet_len - sent);
		if (l == GNUTLS_E_INTERRUPTED)
			continue;

		if (l <= 0) {
			fprintf(stderr, "error send\n");
			return -1;
		}
		sent += l;
	}
	return sent;
}

static int
check_packet_for_ssl(unsigned char *packet, int packet_len)
{
	if (packet_len < 9)
		return 0;

	const int pkt_len = packet_len - 8;
	const unsigned char *const p = packet + 8;
	int i, crypt_flag = 2;
	for (i = 0;; i += 5) {
		unsigned char type;
		int off, len;

		if (i >= pkt_len)
			return 0;
		type = p[i];
		if (type == 0xff)
			break;
		/* check packet */
		if (i+4 >= pkt_len)
			return 0;
		off = p[i+1] * 0x100 + p[i+2];
		len = p[i+3] * 0x100 + p[i+4];
		if (off > pkt_len || (off+len) > pkt_len)
			return 0;
		if (type == 1 && len >= 1)
			crypt_flag = p[off];
	}
	return crypt_flag != 0;
}

static void
hexdump(const unsigned char *buffer, int len)
{
	int i;
	char hex[16 * 3 + 2], chars[20];

	hex[0] = 0;
	for (i = 0; len > 0 && i < len; ++i) {
		const int col = i & 15;
		sprintf(hex + col * 3, "%02x ", (unsigned char) buffer[i]);
		hex[8 * 3 - 1] = '-';
		chars[col] = buffer[i] >= 32 && buffer[i] < 126 ? buffer[i] : '.';
		chars[col + 1] = 0;
		if (col == 15)
			printf("%04x: %-48s %s\n", i & 0xfff0, hex, chars);
	}
	if ((i & 15) != 0)
		printf("%04x: %-48s %s\n", i & 0xfff0, hex, chars);
}

static int
check_port(const char *port)
{
	int n_port = atoi(port);
	if (n_port < 1 || n_port > 0xffff) {
		fprintf(stderr, "Invalid port specified: %s\n", port);
		exit(1);
	}
	return n_port;
}

/* Wait input available on one socket, returns 0 or 1 (which descriptor has data) */
static int
wait_one_fd(int fd1, int fd2)
{
	struct pollfd fds[2];
	fds[0].fd = fd1;
	fds[0].events = POLLIN;
	fds[0].revents = 0;
	fds[1].fd = fd2;
	fds[1].events = POLLIN;
	fds[1].revents = 0;
	for (;;) {
		int num_fds = poll(fds, 2, -1);
		if (num_fds > 0)
			break;
		if (errno == EINTR)
			continue;
		fprintf(stderr, "Error from poll %d\n", errno);
		return -1;
	}
	if (fds[0].revents & (POLLIN|POLLHUP))
		return 0;
	if (fds[1].revents & (POLLIN|POLLHUP))
		return 1;
	fprintf(stderr, "Unexpected event from poll\n");
	return -1;
}

#ifndef _WIN32
static int g_listen_sd = -1;

static void
handle_int_term(int sig)
{
	shutdown(g_listen_sd, SHUT_RDWR);
}
#endif

static void
usage(void)
{
	fprintf(stderr, "bounce [-v] [-d] [-D dump_file] <listen_port> <server> <server_port>\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	int err, listen_sd;
	int ret;
	struct sockaddr_in sa_serv;
	struct sockaddr_in sa_cli;
	socklen_t client_len;
	int optval = 1;
	struct addrinfo hints;
	int ch;
	int listen_port;
	const char *server_ip;
	const char *server_port;

#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

	while ((ch = getopt(argc, argv, "dD:")) != -1) {
		switch (ch) {
		case 'd':
			debug = true;
			break;
		case 'D':
			dump_writer = tcpdump_writer_open(optarg);
			break;
		default:
			usage();
			break;
		}
	}

	if (optind + 3 != argc)
		usage();

	listen_port = check_port(argv[optind]);
	server_port = argv[optind+2];
	check_port(server_port);
	server_ip = argv[optind+1];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_V4MAPPED|AI_ADDRCONFIG;

	ret = getaddrinfo(server_ip, server_port, &hints, &server_addrs);
	if (ret != 0 || server_addrs == NULL) {
		fprintf(stderr, "Invalid server ip specified: %s\n", server_ip);
		exit(1);
	}

	/* this must be called once in the program
	 */
	gnutls_global_init();
	gnutls_global_set_log_level(11);
	if (debug)
		gnutls_global_set_log_function(tds_tls_log);


	gnutls_certificate_allocate_credentials(&x509_cred);

	ret = gnutls_certificate_set_x509_key_file(x509_cred, /* CERTFILE */ CERTFILE, /* KEYFILE */ CERTFILE,
						   GNUTLS_X509_FMT_PEM);
	if (ret < 0) {
		fprintf(stderr, "certificate failed (%s)\n", gnutls_strerror(ret));
		exit(1);
	}

	generate_dh_params();

	gnutls_certificate_set_dh_params(x509_cred, dh_params);

	/* Socket operations
	 */
	listen_sd = socket(AF_INET, SOCK_STREAM, 0);
	SOCKET_ERR(listen_sd, "socket");

	memset(&sa_serv, '\0', sizeof(sa_serv));
	sa_serv.sin_family = AF_INET;
	sa_serv.sin_addr.s_addr = INADDR_ANY;
	sa_serv.sin_port = htons(listen_port);	/* Server Port number */

	setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (void*) &optval, sizeof(int));

	err = bind(listen_sd, (SA *) & sa_serv, sizeof(sa_serv));
	SOCKET_ERR(err, "bind");
	err = listen(listen_sd, 1024);
	SOCKET_ERR(err, "listen");

#ifndef _WIN32
	g_listen_sd = listen_sd;
	{
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));
		sa.sa_flags = SA_RESTART;
		sa.sa_handler = handle_int_term;
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGINT, &sa, NULL);
	}
#endif

	printf("Server ready. Listening to port '%d'.\n\n", listen_port);

	client_len = sizeof(sa_cli);
	for (;;) {
		int client_sd = accept(listen_sd, (SA *) & sa_cli, &client_len);
		if (client_sd < 0)
			break;

		printf("- connection from %s, port %d\n",
		       inet_ntoa(sa_cli.sin_addr), ntohs(sa_cli.sin_port));

		pthread_t th;
		if (pthread_create(&th, NULL, handle_client_thread, (char*) 0 + client_sd)) {
			fprintf(stderr, "Error creating thread\n");
			exit(1);
		}
		pthread_detach(th);
	}
	close(listen_sd);

	gnutls_certificate_free_credentials(x509_cred);
	gnutls_dh_params_deinit(dh_params);
	gnutls_global_deinit();

	freeaddrinfo(server_addrs);

	tcpdump_writer_close(dump_writer);

	return 0;
}

static gnutls_session_t
start_tls_session(unsigned flags, int sd)
{
	gnutls_session_t tls = initialize_tls_session(flags);
	if (!tls)
		return NULL;

	session session;
	memset(&session, 0, sizeof(session));
	session.tls = tls;
	session.sock = sd;

	gnutls_transport_set_ptr(tls, &session);

	int ret = gnutls_handshake(tls);
	if (ret < 0) {
		fprintf(stderr, "*** Handshake has failed (%s)\n\n", gnutls_strerror(ret));
		goto exit;
	}
	if (debug)
		printf("- Handshake was completed\n");

	if (session.to_send) {
		/* flush last packet */
		session.packet[1] = 1;
		if (put_packet(sd, session.packet, session.packet_len) < 0)
			goto exit;
	}

	gnutls_transport_set_ptr(tls, (char*) 0 + sd);
	gnutls_transport_set_pull_function(tls, tds_pull_null_func);
	gnutls_transport_set_push_function(tls, tds_push_null_func);

	return tls;

exit:
	gnutls_deinit(tls);
	return NULL;
}

static tcpdump_flow*
get_dump_flow(void)
{
	static uint16_t next_port = 10000;
	static uint32_t next_ip = 0x0a000002;

	if (!dump_writer)
		return NULL;

	uint16_t port;
	uint32_t ip;
	pthread_mutex_lock(&mutex);
	port = next_port;
	ip = next_ip;
	++next_port;
	if (next_port >= 60000) {
		next_port = 10000;
		++next_ip;
	}
	pthread_mutex_unlock(&mutex);

	return tcpdump_flow_new(ip, port, 0x01020304, 1433);
}

static void
dump_packet(tcpdump_flow* flow, enum tcpdump_flow_direction dir,
	    const void *data, size_t data_size)
{
	if (!dump_writer || !flow)
		return;

	pthread_mutex_lock(&mutex);
	// TODO handle errors
	tcpdump_flow_write_data(dump_writer, flow, dir, data, data_size);
	pthread_mutex_unlock(&mutex);
}

static void
handle_session(int client_sd)
{
	gnutls_session_t client_session = NULL, server_session = NULL;
	int server_sd = -1;
	int use_ssl = 1;
	int ret;
	unsigned char packet[MAX_BUF];
	int packet_len;
	tcpdump_flow *flow = get_dump_flow();

	/* now do prelogin */
	/* connect to real peer */
	printf("connect to real peer\n");
	server_sd = tcp_connect();
	if (server_sd < 0) {
		fprintf(stderr, "Error connecting to server\n");
		goto exit;
	}

	/* get prelogin packet from client */
	printf("get prelogin packet from client\n");
	if ((packet_len = get_packet(client_sd, packet)) <= 0)
		goto exit;
	dump_packet(flow, TCPDUMP_FLOW_CLIENT, packet, packet_len);

	/* send prelogin packet to server */
	printf("send prelogin packet to server\n");
	if (put_packet(server_sd, packet, packet_len) < 0)
		goto exit;

	/* get prelogin reply from server */
	printf("get prelogin reply from server\n");
	if ((packet_len = get_packet(server_sd, packet)) <= 0)
		goto exit;
	dump_packet(flow, TCPDUMP_FLOW_SERVER, packet, packet_len);
	use_ssl = check_packet_for_ssl(packet, packet_len);

	/* reply with same prelogin packet */
	printf("reply with same prelogin packet\n");
	if (put_packet(client_sd, packet, packet_len) < 0)
		goto exit;

	/* now we must do authentication with client and with server */

	/* do with client */
	client_session = start_tls_session(GNUTLS_SERVER, client_sd);
	if (!client_session)
		goto exit;

	/* do with server */
	server_session = start_tls_session(GNUTLS_CLIENT, server_sd);
	if (!server_session)
		goto exit;

	/* now log and do man-in-the-middle to see decrypted data !!! */
	for (;;) {
		/* wait some data */
		ret = wait_one_fd(client_sd, server_sd);
		if (ret == 0) {
			/* client */
			ret = get_packet_tls(client_session, packet, MAX_BUF);
			if (ret > 0) {
				dump_packet(flow, TCPDUMP_FLOW_CLIENT, packet, ret);
				hexdump(packet, ret);
				ret = put_packet_tls(server_session, packet, ret);
			}
			if (!use_ssl)
				break;
		} else if (ret == 1) {
			/* server */
			ret = get_packet_tls(server_session, packet, MAX_BUF);
			if (ret > 0) {
				dump_packet(flow, TCPDUMP_FLOW_SERVER, packet, ret);
				hexdump(packet, ret);
				ret = put_packet_tls(client_session, packet, ret);
			}
		}
		if (ret < 0)
			goto exit;
	}

	for (;;) {
		/* wait some data */
		ret = wait_one_fd(client_sd, server_sd);
		if (ret == 0) {
			/* client */
			if ((packet_len = get_packet(client_sd, packet)) <= 0)
				goto exit;
			if (packet_len > 0)
				dump_packet(flow, TCPDUMP_FLOW_CLIENT, packet, packet_len);
			ret = put_packet(server_sd, packet, packet_len);
		} else if (ret == 1) {
			/* server */
			if ((packet_len = get_packet(server_sd, packet)) <= 0)
				goto exit;
			if (packet_len > 0)
				dump_packet(flow, TCPDUMP_FLOW_SERVER, packet, packet_len);
			ret = put_packet(client_sd, packet, packet_len);
		}
		if (ret < 0)
			goto exit;
	}

exit:
	tcpdump_flow_free(flow);
	if (client_session)
		gnutls_deinit(client_session);
	if (server_session)
		gnutls_deinit(server_session);
	if (server_sd >= 0)
		close(server_sd);
	close(client_sd);
}

static void*
handle_client_thread(void *arg)
{
	handle_session((char*) arg - (char*) 0);
	return NULL;
}
