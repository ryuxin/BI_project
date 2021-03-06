#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mem_mgr.h"
#include "rpc.h"
#include "bi_smr.h"
#include "bi_rcu.h"

struct Mem_layout *global_layout;
struct ps_list_head free_mem_head;
void *malloc_area;

static inline void
__init_magic_str(char *s)
{
	assert(strlen(s) < MAGIC_SZ);
	strcpy(global_layout->magic, s);
}

static inline void *
__init_rpc_rings(struct Per_core_info *rpc, void *addr)
{
	int i, j;
	for(i=0; i<NUM_NODES; i++) {
		for(j=0; j<NUM_CORE_PER_NODE; j++) {
			rpc->info[i][j] = addr;
			msg_queue_struct_init((struct msg_queue *)(rpc->info[i][j]));
			addr += sizeof(struct msg_queue);
		}
	}
	return addr;
}

static inline void *
__init_parsec_times(struct Per_core_info *parsecs, void *addr)
{
	int i, j;
	for(i=0; i<NUM_NODES; i++) {
		for(j=0; j<NUM_CORE_PER_NODE; j++) {
			parsecs->info[i][j] = addr;
			parsec_struct_init((struct parsec *)(parsecs->info[i][j]));
			addr += sizeof(struct parsec);
		}
	}
	return addr;
}

static inline void *
__init_quies_rings(struct Per_node_info *quies, void *addr)
{
	int i;
	for(i=0; i<NUM_NODES; i++) {
		quies->info[i] = addr;
		qsc_ring_struct_init((struct bi_qsc_ring *)(quies->info[i]));
		addr += sizeof(struct bi_qsc_ring);
	}
	return addr;
}

static inline void *
__init_wlogs_rings(struct Per_core_info *quies, void *addr)
{
	int i, j;
	for(i=0; i<NUM_NODES; i++) {
		for(j=0; j<NUM_CORE_PER_NODE; j++) {
			quies->info[i][j] = addr;
			qsc_ring_struct_init((struct bi_qsc_ring *)(quies->info[i][j]));
			addr += sizeof(struct bi_qsc_ring);
		}
	}
	return addr;
}

static inline void *
__init_mem_free_lists(struct Per_node_info *mem_free, void *addr)
{
	int i;
	for(i=0; i<NUM_NODES; i++) {
		mem_free->info[i] = addr;
		addr += sizeof(struct Free_mem_list);
	}
	return addr;
}

static inline void *
__init_mem_start_addr(struct Per_node_info *mem_addr, void *addr)
{
	int i;
	for(i=0; i<NUM_NODES; i++) {
		mem_addr->info[i] = addr;
		addr += (size_t)MEM_MGR_OBJ_SZ * (size_t)MEM_MGR_OBJ_NUM;
	}
	return addr;
}

static inline void
__init_mcs_locks(void)
{
	int i;
	for(i=0; i<MAX_TEST_OBJ_NUM; i++) {
		ck_spinlock_mcs_init(&(global_layout->mcs_lock[i]));
	}
}

void
bi_set_barrier(int k)
{
	global_layout->g_bar.barrier = k;
	clwb_range(&global_layout->g_bar, CACHE_LINE);
}

void
bi_wait_barrier(int k)
{
	int r = 0;
	do {
		clflush_range(&global_layout->g_bar, CACHE_LINE);
		r =  global_layout->g_bar.barrier;
	} while(r != k);
}

void
mem_mgr_init(void)
{
	int i;
	struct Free_mem_list *lh;
	void *addr;

	mem_area_flush();
	lh   = get_mem_free_list(NODE_ID());
	addr = get_mem_start_addr(NODE_ID());
	assert(addr);

	ps_list_head_init(&free_mem_head);
	for(i=0; i<MEM_MGR_OBJ_NUM; i++) {
		lh->mems[i].addr = addr;
		ps_list_init_d(&(lh->mems[i]));
		ps_list_head_append_d(&free_mem_head, &(lh->mems[i]));
		addr += MEM_MGR_OBJ_SZ;
	}
	bi_malloc_init();
}

struct Free_mem_item *
mem_mgr_alloc(size_t sz)
{
	struct Free_mem_item *ret;

	assert(sz == MEM_MGR_OBJ_SZ);
	assert(!ps_list_head_empty(&free_mem_head));
	ret = ps_list_head_first_d(&free_mem_head, struct Free_mem_item);
	assert(ret);
	ps_list_rem_d(ret);

	return ret;
}

void
mem_mgr_free(struct Free_mem_item *buf)
{
	ps_list_head_append_d(&free_mem_head, buf);
}

void *
init_global_memory(void *global_memory, char *s, void **data_area)
{
	void *addr;
	int i;
	
	addr = global_memory;
	memset(global_layout, 0, sizeof(struct Mem_layout));
	if (s) __init_magic_str(s);
	
	addr = (void *)round_up_to_page(addr + sizeof(struct Mem_layout));
	global_layout->rpc_area = addr;
	for(i=0; i<NUM_NODES; i++) {
		addr = __init_rpc_rings(&global_layout->send_rings[i], addr);
	}
	for(i=0; i<NUM_NODES; i++) {
		addr = __init_rpc_rings(&global_layout->recv_rings[i], addr);
	}
	
	addr = (void *)round_up_to_page(addr);
	global_layout->parsec_area = addr;
	addr = __init_parsec_times(&global_layout->parsec_times, addr);

	addr = (void *)round_up_to_page(addr);
	global_layout->quies_area = addr;
	addr = __init_quies_rings(&global_layout->quies_rings, addr);

	addr = (void *)round_up_to_page(addr);
	global_layout->wlog_area = addr;
	addr = __init_wlogs_rings(&global_layout->wlog_rings, addr);

	__init_mcs_locks();
	addr = (void *)round_up_to_page(addr);
	global_layout->test_obj_area = addr;
	addr = addr + (MAX_TEST_OBJ_NUM * sizeof(struct Test_obj));

	addr = (void *)round_up_to_page(addr);
	global_layout->mem_list_area = addr;
	addr = __init_mem_free_lists(&global_layout->mem_free_lists, addr);

	addr = (void *)round_up_to_page(addr);
	global_layout->rcu_area = addr;
	addr = addr + sizeof(struct RCU_block);

	addr = (void *)round_up_to_page(addr);
	global_layout->data_area = addr;
	addr = __init_mem_start_addr(&global_layout->mem_start_addr, addr);
	*data_area = global_layout->data_area;
	
	__init_mem_start_addr(&global_layout->malloc_start_addr, malloc_area);

	global_layout->g_bar.barrier = 0;
	for(i=0; i<NUM_NODES; i++) {
		global_layout->bars[i].barrier = 0;
	}

	printf("magic string %s\n", global_layout->magic);
	printf("start %p end %p tot sz %lu\n", global_memory, addr, (unsigned long)(addr - global_memory));
	return addr;
}
