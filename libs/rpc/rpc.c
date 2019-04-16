#include <stdio.h>
#include <stdlib.h>
#include "bi_pointer.h"
#include "mem_mgr.h"
#include "rpc.h"

typedef enum {
	NOFLUSH,
	FLUSH,
} flush_op_t;

static struct local_pos snt_pos, rcv_pos;
static int svr_node, svr_core;

static inline void
dump_msg_queue(struct msg_queue *q)
{
	int i;
	struct msg_node *mn;
	for(i=0; i<MSG_NUM; i++) {
		mn = &(q->ring[i]);
		printf("dbg dump %p i %d use %d size %lu\n", q, i, mn->meta.use, mn->meta.size);
	}
}

static inline void
__server_flush_msgq(int nn, int nc)
{
//return ;
	int i, j;
        struct msg_queue *mq;

	for(i=0; i<nn; i++) {
		for(j=0; j<nc; j++) {
			mq  = (struct msg_queue *)get_recv_ring_server(i, j);
			clflush_range_opt(mq, sizeof(struct msg_queue));
		}
	}
	bi_wmb();
}

static inline void
__advance_recv_id(void)
{
	int nn, nc;

	nn = get_active_node_num();
	nc = get_active_core_num();
	if (svr_node == 0 && svr_core == 0) {
		__server_flush_msgq(nn, nc);
	}
	svr_core++;
	if (svr_core % nc == 0) {
		svr_core = 0;
		svr_node = (svr_node+1) % nn;
	}
}

static inline int
rpc_send_ext(struct msg_queue *q, int *pos, void *data, size_t size)
{
	struct msg_node *mn;
	int cur = (*pos) % MSG_NUM;

	mn = &(q->ring[cur]);
//	clflush_range(&mn->meta, CACHE_LINE);
//	bi_inst_bar();
//	if (unlikely(mn->meta.use)) return -1;
	memcpy(mn->data, data, size);
	clwb_range(mn->data, size);
	mn->meta.size = size;
	mn->meta.use  = 1;
	clwb_range(&mn->meta, CACHE_LINE);
	*pos = cur + 1;
	return 0;
}

static inline size_t
rpc_recv_ext(struct msg_queue *q, int *pos, void *data, int spin, flush_op_t nd_flush)
{
	struct msg_node *mn;
	size_t ret_sz = 0;
	int cur = (*pos) % MSG_NUM;

	do {
		mn = &(q->ring[cur]);
		if (nd_flush == FLUSH) {
			clflush_range_opt(&mn->meta, CACHE_LINE);
			bi_inst_bar();
		}
		if (!mn->meta.use) continue;
		bi_ccb();
		ret_sz = mn->meta.size;
		if (nd_flush == FLUSH) {
			clflush_range(mn->data, ret_sz);
			bi_inst_bar();
		}
		memcpy(data, mn->data, ret_sz);
//		bi_dereference_area_aggressive(data, mn->data, ret_sz);
		mn->meta.size = 0;
		mn->meta.use  = 0;
		clwb_range(&mn->meta, CACHE_LINE);
		*pos = cur + 1;
	} while (!ret_sz && spin);
	assert(ret_sz < MAX_MSG_SIZE);

	return ret_sz;
}

int
rpc_send(int node, void *data, size_t size)
{
	struct msg_queue *mq;
	int *pos;

//	printf("rpc send sender %d to %d data %p sz %lu\n", CORE_ID(), node, data, size);
	assert(size < MAX_MSG_SIZE);
	mq  = (struct msg_queue *)get_send_ring(node);
	assert(mq);
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

	return rpc_recv_ext(mq, pos, data, spin, FLUSH);
}

int
rpc_send_server(int nid, int cid, void *data, size_t size)
{
	struct msg_queue *mq;
	int *pos;

//	printc("rpc send sender %d to %d id %d sz %d\n", caller, recv_node, memid, size);
	assert(size < MAX_MSG_SIZE);
	mq  = (struct msg_queue *)get_send_ring_server(nid, cid);
	pos = &(rcv_pos.head[nid][cid]);

	return rpc_send_ext(mq, pos, data, size);
}

size_t
rpc_recv_server(void *data, int *nid, int *cid)
{
	struct msg_queue *mq;
	int *pos;
	size_t ret_sz = 0;
	
	mq  = (struct msg_queue *)get_recv_ring_server(svr_node, svr_core);
	pos = &(snt_pos.tail[svr_node][svr_core]);

	ret_sz = rpc_recv_ext(mq, pos, data, 0, NOFLUSH);
	*nid = svr_node;
	*cid = svr_core;
	__advance_recv_id();
	return ret_sz;
}

void
rpc_init_global()
{
	memset(&snt_pos, 0, sizeof(struct local_pos));
	memset(&rcv_pos, 0, sizeof(struct local_pos));
	svr_node = svr_core = 0;
}

