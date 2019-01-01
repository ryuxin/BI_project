#ifndef MEM_MGR_H
#define MEM_MGR_H

#include <ck_spinlock.h>
#include "constant.h"
#include "hw_util.h"
#include "ps_list.h"

#define MAGIC_SZ 64

struct mcslock_context {
	ck_spinlock_mcs_context_t l;
	char padding[CACHE_LINE*2 - sizeof(ck_spinlock_mcs_context_t)];
} __attribute__((aligned(CACHE_LINE)));

struct Test_obj {
	void *data;
	size_t sz;
} __attribute__((aligned(CACHE_LINE)));

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
	volatile uint64_t tsc;
} __attribute__((aligned(CACHE_LINE)));

struct Global_barrier {
	volatile int barrier;
} __attribute__((aligned(CACHE_LINE)));

struct Mem_layout {
	char magic[MAGIC_SZ];
	void *rpc_area;
	void *parsec_area;
	void *quies_area;
	void *mem_list_area;
	void *rcu_area;
	struct Test_obj *test_obj_area;
	void *data_area;
	struct Per_core_info send_rings[NUM_NODES];
	struct Per_core_info recv_rings[NUM_NODES];
	struct Per_core_info parsec_times;
	struct Per_node_info quies_rings;
	struct Per_node_info mem_free_lists;
	struct Per_node_info mem_start_addr;
	struct Global_rdtsc time;
	struct Global_barrier g_bar;
	struct mcslock_context mcs_cntxt[NUM_NODES][NUM_CORE_PER_NODE];
	ck_spinlock_mcs_t mcs_lock[MAX_TEST_OBJ_NUM];
} __attribute__((aligned(CACHE_LINE), packed));

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

static inline ck_spinlock_mcs_t
get_mcs_lock_cntxt(void)
{
	return &(global_layout->mcs_cntxt[NODE_ID()][CORE_ID()].l);
}

static inline struct ck_spinlock_mcs **
get_mcs_lock(int id)
{
	return &(global_layout->mcs_lock[id]);
}

static inline struct Test_obj *
get_test_obj(int id)
{
	return &(global_layout->test_obj_area[id]);
}

static inline void *
get_rcu_area(void)
{
	return global_layout->rcu_area;
}

/************** debug functions ***********/
static inline void
dbg_chk_per_core(struct Per_core_info *p)
{
	int i, j;
	assert(p);
	for(i=0; i<NUM_NODES; i++) {
		for(j=0; j<NUM_CORE_PER_NODE; j++) {
			assert(p->info[i][j]);
		}
	}
}
static inline void
dbg_chk_per_node(struct Per_node_info *p)
{
	int i;
	assert(p);
	for(i=0; i<NUM_NODES; i++) {
		assert(p->info[i]);
	}
}

void bi_set_barrier(int k);
void bi_wait_barrier(int k);
/* This is called only once by master node */
void *init_global_memory(void *global_memory, char *s);
/* This is called by every node to init phy mem allocator */
void mem_mgr_init();
struct Free_mem_item *mem_mgr_alloc(size_t sz);
void mem_mgr_free(struct Free_mem_item *buf);
/* malloc style allocation, while thread-safe,
 * should not used with above phy mem allocator */
void bi_malloc_init(void);
void *bi_malloc(size_t size);
void bi_free(void *ptr);

#endif /* MEM_MGR_H */
