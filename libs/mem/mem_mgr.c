#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mem_mgr.h"
#include "rpc.h"

struct Mem_layout *global_layout;

static inline void
__init_magic_str(char *s)
{
	assert(strlen(s) < MAGIC_SZ);
	strcpy(global_layout->magic, s);
}

static inline void *
__init_rpc_rings(struct RPC_rings *rpc, void *addr)
{
	int i, j;
	for(i=0; i<NUM_NODES; i++) {
		for(j=0; j<NUM_CORE_PER_NODE; j++) {
			rpc->rings[i][j] = addr;
			addr += sizeof(struct msg_queue);
		}
	}
	return addr;
}

/***************TODO: init mem layout parsec time, quiense queue***********/
void
init_global_memory(void *global_memory, char *s)
{
	void *addr;
	int i;
	
	addr = global_memory;
	global_layout = (struct Mem_layout *)addr;
	__init_magic_str(s);
	
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

	printf("magic string %s\n", global_layout->magic);
	printf("start %p end %p tot sz %lu\n", global_memory, addr, (unsigned long)(addr - global_memory));
}
