#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "dump_write.h"

#if !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__) \
    || !defined(__ORDER_BIG_ENDIAN__) || !defined(__ORDER_PDP_ENDIAN__)
# error Implement this
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define NTOHS_CONST(n) ((((n)&0xff)<<8)|(((n)>>8)&0xff))
#else
#define NTOHS_CONST(n) (n)
#endif

#if defined(__GNUC__)
#  define MAY_ALIAS __attribute__((__may_alias__))
#else
#  define MAY_ALIAS
#endif

static inline unsigned
reduce_cksum(unsigned sum)
{
	return (sum >> 16) + (sum & 0xffffu);
}

static unsigned
cksum(const void *pkt, size_t len, unsigned int start)
{
	typedef union MAY_ALIAS { uint16_t full; uint8_t part; } u16;
	const u16 *data = (const u16 *) pkt;
	unsigned sum = start;

	for (; len >= 2; len -= 2, ++data)
		sum += data->full;
	if (len)
		sum += ntohs(data->part << 8);
	sum = reduce_cksum(sum);
	sum = reduce_cksum(sum);
	return sum;
}

typedef struct ip_header {
	uint8_t ip_verlen; // 0x45
	uint8_t ip_tos;
	uint16_t ip_len;
	uint16_t ip_id;
	uint16_t ip_off;
	uint8_t ip_ttl;
	uint8_t ip_proto;
	uint16_t ip_sum;
	uint32_t ip_src, ip_dst;
} ip_header;

typedef struct tcp_header {
	uint16_t th_sport;
	uint16_t th_dport;
	uint32_t th_seq;
	uint32_t th_ack;
	uint8_t th_lenres; // 0x50
	uint8_t th_flags;
	uint16_t th_win;
	uint16_t th_sum;
	uint16_t th_urp;
} tcp_header;

typedef struct tcp_packet_header {
	ip_header ip;
	tcp_header tcp;
} tcp_packet_header;

static const tcp_packet_header base_header = {
	{ 0x45, 0, 0, NTOHS_CONST(0x1234), 0, 64, 6 /* TCP */, 0, 0, 0 },
	{ 0, 0, 0, 0, 0x50, 0x10 /* flags, ACK */ , NTOHS_CONST(0x2000), 0, 0 }
};

#define LINKTYPE_RAW	101

typedef struct {
	uint32_t magic_number;   /* magic number */
	uint16_t version_major;  /* major version number */
	uint16_t version_minor;  /* minor version number */
	int32_t  thiszone;       /* GMT to local correction */
	uint32_t sigfigs;        /* accuracy of timestamps */
	uint32_t snaplen;        /* max length of captured packets, in octets */
	uint32_t network;        /* data link type */
} pcap_hdr;

typedef struct {
	uint32_t ts_sec;         /* timestamp seconds */
	uint32_t ts_usec;        /* timestamp microseconds */
	uint32_t incl_len;       /* number of octets of packet saved in file */
	uint32_t orig_len;       /* actual length of packet */
} pcaprec_hdr;

struct tcpdump_writer
{
	FILE *file;
	char buf[1024 * 16];
};

struct tcpdump_flow
{
	uint32_t saddr, daddr;
	uint16_t sport, dport;
	uint32_t sseq_sum, dseq_sum;
	unsigned pseudo_cksum;
};

tcpdump_writer* tcpdump_writer_open(const char *fn)
{
	FILE *f = fopen(fn, "wb");
	if (!f)
		return NULL;

	tcpdump_writer *writer = calloc(1, sizeof(tcpdump_writer));
	if (!writer) {
		fclose(f);
		unlink(fn);
		return NULL;
	}
	setvbuf(f, writer->buf, _IOFBF, sizeof(writer->buf));

	// write file header
	pcap_hdr hdr;
	hdr.magic_number = 0xa1b2c3d4;
	hdr.version_major = 2;
	hdr.version_minor = 4;
	hdr.thiszone = 0;
	hdr.sigfigs = 0;
	hdr.snaplen = 65535;
	hdr.network = LINKTYPE_RAW;

	if (fwrite(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
		fclose(f);
		unlink(fn);
		free(writer);
		return NULL;
	}

	writer->file = f;
	return writer;
}

void tcpdump_writer_close(tcpdump_writer* dump)
{
	if (!dump)
		return;
	fclose(dump->file);
	free(dump);
}

tcpdump_flow* tcpdump_flow_new(uint32_t saddr, uint16_t sport,
			       uint32_t daddr, uint16_t dport)
{
	tcpdump_flow *flow = calloc(1, sizeof(tcpdump_flow));
	if (!flow)
		return NULL;

	saddr = htonl(saddr);
	daddr = htonl(daddr);
	sport = htons(sport);
	dport = htons(dport);

	flow->saddr = saddr;
	flow->daddr = daddr;
	flow->sport = sport;
	flow->dport = dport;

	const struct {
		uint32_t saddr, daddr;
		uint8_t zero, proto;
	} pseudo = { saddr, daddr, 0, 6 };
	flow->pseudo_cksum = cksum(&pseudo, 10, 0);

	return flow;
}

void tcpdump_flow_free(tcpdump_flow *flow)
{
	free(flow);
}

int tcpdump_flow_write_data(tcpdump_writer* writer,
			    tcpdump_flow *flow,
			    enum tcpdump_flow_direction direction,
			    const void *data, size_t data_size)
{
	enum { MAX_PACKET = 1460 };

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	// prepare base headers
	struct {
		pcaprec_hdr file;
		tcp_packet_header pkt;
	} hdrs;
	hdrs.file.ts_sec = ts.tv_sec;
	hdrs.file.ts_usec = ts.tv_nsec / 1000;
	hdrs.pkt = base_header;

	size_t pos = 0;
	while (pos < data_size) {
		const uint32_t len =
			data_size - pos > MAX_PACKET ? MAX_PACKET : data_size - pos;

		// prepare headers
		hdrs.file.incl_len = len + sizeof(hdrs.pkt);
		hdrs.file.orig_len = len + sizeof(hdrs.pkt);
		hdrs.pkt.ip.ip_len = htons(len + sizeof(hdrs.pkt));

		if (direction == TCPDUMP_FLOW_CLIENT) {
			hdrs.pkt.ip.ip_src = flow->saddr;
			hdrs.pkt.ip.ip_dst = flow->daddr;
			hdrs.pkt.tcp.th_sport = flow->sport;
			hdrs.pkt.tcp.th_dport = flow->dport;
			hdrs.pkt.tcp.th_seq = htonl(flow->sseq_sum);
			hdrs.pkt.tcp.th_ack = htonl(flow->dseq_sum);
			flow->sseq_sum += len;
		} else {
			hdrs.pkt.ip.ip_src = flow->daddr;
			hdrs.pkt.ip.ip_dst = flow->saddr;
			hdrs.pkt.tcp.th_sport = flow->dport;
			hdrs.pkt.tcp.th_dport = flow->sport;
			hdrs.pkt.tcp.th_seq = htonl(flow->dseq_sum);
			hdrs.pkt.tcp.th_ack = htonl(flow->sseq_sum);
			flow->dseq_sum += len;
		}

		// compute checksums
		hdrs.pkt.ip.ip_sum = 0;
		hdrs.pkt.ip.ip_sum = cksum(&hdrs.pkt.ip, sizeof(hdrs.pkt.ip), 0) ^ 0xffff;
		hdrs.pkt.tcp.th_sum = 0;
		unsigned sum = flow->pseudo_cksum + htons(len + sizeof(hdrs.pkt.tcp));
		sum = cksum(&hdrs.pkt.tcp, sizeof(hdrs.pkt.tcp), sum);
		sum = cksum(data, len, sum);
		hdrs.pkt.tcp.th_sum = sum ^ 0xffff;

		if (fwrite(&hdrs, 1, sizeof(hdrs), writer->file) != sizeof(hdrs))
			return -1;
		if (fwrite(data, 1, len, writer->file) != len)
			return -1;

		data = (const uint8_t*) data + len;
		pos += len;
	}
	return data_size;
}

#ifdef TEST_DUMP_WRITE
#include <assert.h>

int main(void)
{
	tcpdump_writer* writer = tcpdump_writer_open("test.pcap");
	assert(writer);

	tcpdump_flow* flow = tcpdump_flow_new(0x01020304, 1433, 0xa1b2c3d4, 0x8765);
	assert(flow);

	tcpdump_flow_write_data(writer, flow, TCPDUMP_FLOW_CLIENT, "\0\0\0\0\0\0\0\0", 8);
	tcpdump_flow_write_data(writer, flow, TCPDUMP_FLOW_CLIENT, "test123", 8);
	tcpdump_flow_write_data(writer, flow, TCPDUMP_FLOW_CLIENT, "test123", 8);
	tcpdump_flow_write_data(writer, flow, TCPDUMP_FLOW_SERVER, "test123", 7);
	tcpdump_flow_write_data(writer, flow, TCPDUMP_FLOW_SERVER, "test123", 7);
	tcpdump_flow_free(flow);

	tcpdump_writer_close(writer);
}
#endif
