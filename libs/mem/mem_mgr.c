#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mem_mgr.h"
#include "rpc.h"
#include "bi_smr.h"

struct Mem_layout *global_layout;
struct ps_list_head free_mem_head;

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
	int i, j;
	for(i=0; i<NUM_NODES; i++) {
		quies->info[i] = addr;
		qsc_ring_struct_init((struct bi_qsc_ring *)(quies->info[i]));
		addr += sizeof(struct bi_qsc_ring);
	}
	return addr;
}

static inline void *
__init_mem_free_lists(struct Per_node_info *mem_free, void *addr)
{
	int i, j;
	for(i=0; i<NUM_NODES; i++) {
		mem_free->info[i] = addr;
		addr += sizeof(struct Free_mem_list);
	}
	return addr;
}

static inline void *
__init_mem_start_addr(struct Per_node_info *mem_addr, void *addr)
{
	int i, j;
	for(i=0; i<NUM_NODES; i++) {
		mem_addr->info[i] = addr;
		addr += MEM_MGR_OBJ_SZ * MEM_MGR_OBJ_NUM;
	}
	return addr;
}

void
mem_mgr_init(void)
{
	int i;
	struct Free_mem_list *lh;
	void *addr;

	lh   = get_mem_free_list(NODE_ID());
	addr = get_mem_start_addr(NODE_ID());
	assert(start_addr);

	ps_list_head_init(&free_mem_head);
	for(i=0; i<MEM_MGR_OBJ_NUM; i++) {
		lh->mems[i].addr = addr;
		ps_list_init_d(&(lh->mems[i]));
		ps_list_head_append_d(&free_mem_head, &(lh->mems[i]));
		addr += MEM_MGR_OBJ_SZ;
	}
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

void
init_global_memory(void *global_memory, char *s)
{
	void *addr;
	int i;
	
	addr = global_memory;
	global_layout = (struct Mem_layout *)addr;
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
	global_layout->mem_list_area = addr;
	addr = __init_mem_free_lists(&global_layout->mem_free_lists, addr);

	addr = (void *)round_up_to_page(addr);
	global_layout->data_area = addr;
	addr = __init_mem_start_addr(&global_layout->mem_start_addr, addr)

	printf("magic string %s\n", global_layout->magic);
	printf("start %p end %p tot sz %lu\n", global_memory, addr, (unsigned long)(addr - global_memory));
}
