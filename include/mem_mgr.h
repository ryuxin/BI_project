#ifndef MEM_MGR_H
#define MEM_MGR_H

#include "constant.h"
#include "hw_util.h"

#define MAGIC_SZ 64

/***************** TODO: read enter/exit timestapm, quiense queue, rpc memory, data memory init ***********/
struct RPC_rings {
	void *rings[NUM_NODES][NUM_CORE_PER_NODE];
} __attribute__((aligned(CACHE_LINE), packed));

struct Mem_layout {
	char magic[MAGIC_SZ];
	void *parsec_area;
	void *rpc_area;
	void *parsec_times[NUM_NODES][NUM_CORE_PER_NODE];
	struct RPC_rings send_rings[NUM_NODES];
	struct RPC_rings recv_rings[NUM_NODES];
};

extern struct Mem_layout *global_layout;

static inline void *
get_send_ring(int nid)
{
	return global_layout->send_rings[nid].rings[NODE_ID()][CORE_ID()];
}

static inline void *
get_recv_ring(int nid)
{
	return global_layout->recv_rings[nid].rings[NODE_ID()][CORE_ID()];
}

static inline void *
get_send_ring_server(int nid, int cid)
{
	return global_layout->recv_rings[NODE_ID()].rings[nid][cid];
}

static inline void *
get_recv_ring_server(int nid, int cid)
{
	return global_layout->send_rings[NODE_ID()].rings[nid][cid];
}

void init_global_memory(void *global_memory, char *s);

#endif /* MEM_MGR_H */
