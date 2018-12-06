#ifndef RPC_H
#define RPC_H

#include "constant.h"

struct msg_meta {
	size_t size;   /* message size */
	int use;
} __attribute__((aligned(CACHE_LINE), packed));

struct msg_node {
	struct msg_meta meta;
	char data[MAX_MSG_SIZE];
} __attribute__((aligned(CACHE_LINE), packed));

struct msg_queue {
	struct msg_node ring[MSG_NUM];
} __attribute__((aligned(CACHE_LINE), packed));

struct local_pos {
	int head[NUM_NODES][NUM_CORE_PER_NODE];
	int tail[NUM_NODES][NUM_CORE_PER_NODE];
} __attribute__((aligned(CACHE_LINE), packed));

static inline size_t
rpc_area_tot_sz()
{
	return 2*NUM_NODES*NUM_NODES*NUM_CORE_PER_NODE*sizeof(struct msg_queue);
}

/* clients send a message with data and size to server on node */
int rpc_send(int node, void *data, size_t size);
/* clients receive a message from server on node, return size and copy message to data */ 
size_t rpc_recv(int node, void *data, int spin);
/* servers send data with size to (nid, cid)*/
int rpc_send_server(int nid, int cid, void *data, size_t size);
/* servers receive loop, return size, sender (nid, cid) and copy message to data */ 
size_t rpc_recv_server(void *data, int *nid, int *cid);
void rpc_init_global();

#endif /* RPC_H */
