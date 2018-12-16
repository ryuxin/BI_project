#ifndef MEM_MGR_H
#define MEM_MGR_H

#include "constant.h"
#include "hw_util.h"
#include "ps_list.h"

#define MAGIC_SZ 64

struct Free_mem_item {
	void *addr;
	struct ps_list list;
} __attribute__((packed));

struct Free_mem_list {
	struct Free_mem_item mems[MEM_MGR_OBJ_NUM];
} __attribute__((packed));

struct Per_core_info {
	void *info[NUM_NODES][NUM_CORE_PER_NODE];
} __attribute__((aligned(CACHE_LINE), packed));

struct Per_node_info {
	void *info[NUM_NODES];
} __attribute__((aligned(CACHE_LINE), packed));

struct Global_rdtsc {
	uint64_t tsc;
} __attribute__((aligned(CACHE_LINE)));

struct Mem_layout {
	char magic[MAGIC_SZ];
	void *rpc_area;
	void *parsec_area;
	void *quies_area;
	void *mem_list_area;
	void *data_area;
	struct Per_core_info send_rings[NUM_NODES];
	struct Per_core_info recv_rings[NUM_NODES];
	struct Per_core_info parsec_times;
	struct Per_node_info quies_rings;
	struct Per_node_info mem_free_lists;
	struct Per_node_info mem_start_addr;
	struct Global_rdtsc time;
};

extern struct Mem_layout *global_layout;

static inline void *
get_send_ring(int nid)
{
	return global_layout->send_rings[nid].info[NODE_ID()][CORE_ID()];
}

static inline void *
get_recv_ring(int nid)
{
	return global_layout->recv_rings[nid].info[NODE_ID()][CORE_ID()];
}

static inline void *
get_send_ring_server(int nid, int cid)
{
	return global_layout->recv_rings[NODE_ID()].info[nid][cid];
}

static inline void *
get_recv_ring_server(int nid, int cid)
{
	return global_layout->send_rings[NODE_ID()].info[nid][cid];
}

static inline void *
get_parsec_time(int nid, int cid)
{
	return global_layout->parsec_times.info[nid][cid];
}

static inline void *
get_quies_ring(int nid)
{
	return global_layout->quies_rings.info[nid];
}

static inline void *
get_mem_free_list(int nid)
{
	return global_layout->mem_free_lists.info[nid];
}

static inline void *
get_mem_start_addr(int nid)
{
	return global_layout->mem_start_addr.info[nid];
}

/* This is called only once by master node */
void init_global_memory(void *global_memory, char *s);
/* This is called by every node to init phy mem allocator */
void mem_mgr_init();
struct Free_mem_item *mem_mgr_alloc(size_t sz);
void mem_mgr_free(struct Free_mem_item *buf);

#endif /* MEM_MGR_H */
