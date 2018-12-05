#include "rpc.h"
#include "bi_pointer.h"
#include "mem_mgr.h"

static struct local_pos snt_pos, rcv_pos;

static inline int
rpc_send_ext(struct msg_queue *q, int *pos, void *data, size_t size)
{
	struct msg_node *mn;
	int cur = (*pos) % MSG_NUM;

	mn = &(q->ring[cur]);
	bi_flush_cache(&mn->meta);
	if (unlikely(mn->meta.use)) return -1;
	bi_publish_area(mn->data, data, size);
	mn->meta.size = size;
	mn->meta.use  = 1;
	bi_wb_cache(&mn->meta);
	*pos = cur + 1;
	return 0;
}

static inline size_t
rpc_recv_ext(struct msg_queue *q, int *pos, void *data, int spin)
{
	struct msg_node *mn;
	size_t ret_sz = 0;
	int cur = (*pos) % MSG_NUM;

	do {
		mn = &(q->ring[cur]);
		bi_flush_cache(&mn->meta);
		if (!mn->meta.use) break;
		ret_sz = mn->meta.size;
		bi_dereference_area_aggressive(data, mn->data, ret_sz);
		mn->meta.size = 0;
		mn->meta.use  = 0;
		bi_wb_cache(&mn->meta);
		*pos = cur + 1;
	} while (!spin);

	return ret_sz;
}

int
rpc_send(int node, void *data, size_t size);
{
	struct msg_queue *mq;
	int *pos;

//	printc("rpc send sender %d to %d id %d sz %d\n", caller, recv_node, memid, size);
	mq  = (struct msg_queue *)get_send_ring(node);
	pos = &(snt_pos.head[node][CORE_ID()]);

	return rpc_send_ext(mq, pos, data, size);
}

size_t
rpc_recv(int node, void *data, int spin)
{
	struct msg_queue *mq;
	int *pos;

//	printc("rpc recv node %d\n", caller);
	mq  = (struct msg_queue *)get_recv_ring(node);
	pos = &(rcv_pos.tail[node][CORE_ID()]);

	return rpc_send_ext(mq, pos, data, size);
}

/***************  TODO sever side***/
/******************  FIXME **********/

/*
size_t
rpc_recv(int node, void *data, int spin)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16;
	struct recv_ret *ret = (struct recv_ret *)ret_page[caller].addr;
	int deq, i;
	struct msg_meta meta;
	struct mem_meta * mem;
	void *addr;

//	printc("rpc recv node %d\n", caller);
	do {
		for(i=(caller+1)%NUM_NODE; i!=caller; i = (i+1)%NUM_NODE) {
#ifdef NO_HEAD
			deq = msg_dequeue(&global_msg_pool.nodes[caller].recv[i], &rcv_pos[caller].pos[i], &meta);
#else
			deq = msg_dequeue(&global_msg_pool.nodes[caller].recv[i], &meta);
#endif
			if (!deq) {
				ret->mem_id = meta.mem_id;
				ret->size   = meta.size;
				ret->sender = i;
				ret->addr   = mem_retrieve(meta.mem_id, caller);
				mem         = mem_lookup(meta.mem_id);
				assert(meta.size <= mem->size);
				addr = (void *)mem->addr;
				clflush_range(addr, addr+meta.size);
				return ret_page[caller].dst;
			}
		}
	} while(spin);
	return NULL;
}
*/

void
rpc_init(int node_mem, vaddr_t untype, int size)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16;
	int i, j;
	vaddr_t vas = (vaddr_t)cos_get_heap_ptr()+PAGE_SIZE+COST_ARRAY_NUM_PAGE*PAGE_SIZE;
	vas = round_up_to_pgd_page(vas);

	printc("rpc init node %d addr %x size %x vas %x\n", caller, untype, size, vas);
	mem_mgr_init(untype, size, vas);
	memset((void *)&global_msg_pool, 0, sizeof(struct msg_pool));
#ifdef NO_HEAD
	memset((void *)&snt_pos, 0, sizeof(snt_pos));
	memset((void *)&rcv_pos, 0, sizeof(rcv_pos));
#endif
}

