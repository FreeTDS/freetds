#ifndef dump_write_h_eOjyViTDpnYyJ8gBhnGtqv
#define dump_write_h_eOjyViTDpnYyJ8gBhnGtqv 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tcpdump_writer tcpdump_writer;
typedef struct tcpdump_flow tcpdump_flow;

enum tcpdump_flow_direction {
	TCPDUMP_FLOW_CLIENT,
	TCPDUMP_FLOW_SERVER
};

// Open a TCP dump file.
// A TCP dump file is used to dump TCP streams.
// Returns a new dumper or NULL on error.
tcpdump_writer* tcpdump_writer_open(const char *fn);

// Close a TCP dump.
// After closing pointer and file get invalid.
void tcpdump_writer_close(tcpdump_writer* dump);

// Create a new flow.
// Returns a new flow dumper or NULL on error.
tcpdump_flow* tcpdump_flow_new(uint32_t saddr, uint16_t sport,
			       uint32_t daddr, uint16_t dport);

// Destroy a flow.
void tcpdump_flow_free(tcpdump_flow *flow);

// Write data for a specific flow.
int tcpdump_flow_write_data(tcpdump_writer* writer,
			    tcpdump_flow *flow,
			    enum tcpdump_flow_direction direction,
			    const void *data, size_t data_size);

#ifdef __cplusplus
}
#endif

#endif // dump_write_h_eOjyViTDpnYyJ8gBhnGtqv
