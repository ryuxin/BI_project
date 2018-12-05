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

/* 
 * This is core local. Server core may use all of them, while 
 * other client cores only use entries associated with servers.
 */
struct local_pos {
	int head[NUM_NODES][NUM_CORE_PER_NODE];
	int tail[NUM_NODES][NUM_CORE_PER_NODE];
} __attribute__((aligned(CACHE_LINE), packed));

/* send a message with data and size to (node, core) */
int rpc_send(int node, void *data, size_t size);
/* receive a message from (node, core), return size and copy message to data */ 
size_t rpc_recv(int node, void *data, int spin);
void rpc_init_global();
void rpc_init_local();

#endif /* RPC_H */
