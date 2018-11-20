#ifndef RPC_H
#define RPC_H

#include "constant.h"
#include "util.h"

#define MSG_NUM 8
#define DELAY 500UL

typedef enum {
	MC_MSG_GET,
	MC_MSG_SET,
	MC_MSG_GET_OK,
	MC_MSG_GET_FAIL,
	MC_MSG_SET_OK,
	MC_MSG_SET_FAIL,
	MC_MSG_PREPARE,
	MC_MSG_PREPARE_OK,
	MC_MSG_COMMIT,
	MC_MSG_COMMIT_OK,
	MC_MEM_REQ,
	MC_MEM_REPLY,
	MC_BEGIN,
	MC_EXIT
} mc_message_t;

struct mc_msg {
	mc_message_t type;
	int nkey, nbytes, id;
	uint32_t hv;
	int key_off, data_off;
	char *key, *data;
	uint32_t evict_hv;
};

struct msg_meta {
	int size, off;   /* message size */
} __attribute__((aligned(CACHE_LINE), packed));

struct msg_queue {
	int head;
	char pad[2*CACHE_LINE-sizeof(int)];
	int tail;
	char _pad[2*CACHE_LINE-sizeof(int)];
	struct msg_meta ring[MSG_NUM];
} __attribute__((aligned(2*CACHE_LINE), packed));

struct recv_queues {
	struct msg_queue recv[NUM_NODE];
} __attribute__((aligned(CACHE_LINE)));

struct msg_pool {
	struct recv_queues nodes[NUM_NODE];
	int done;
};

static struct msg_pool *global_msg_pool;
static void * msg_buf[NUM_NODE][NUM_NODE][2];

/* single producer single consumer queue */
static inline int
msg_enqueue(struct msg_queue *q, struct msg_meta *entry)
{
	int consumer, producer, delta;

	consumer = cc_load_int(&q->head);	
	producer = q->tail;
	delta    = (producer + 1)%MSG_NUM;
	if (delta == consumer) {
		consumer = non_cc_load_int(&q->head);
		if (delta == consumer) return -1;
	}
	q->ring[producer] = *entry;
	clwb_range(&(q->ring[producer]), (char *)&(q->ring[producer]) + CACHE_LINE);
	non_cc_store_int(&q->tail, delta);
	return 0;
}

static inline int
msg_dequeue(struct msg_queue *q, struct msg_meta *entry)
{
	int consumer, producer;

	consumer = q->head;
	producer = cc_load_int(&q->tail);
	if (consumer == producer) {
		producer = non_cc_load_int(&q->tail);
		if (consumer == producer) return -1;
	}
	clflush_range(&q->ring[consumer], (char *)&q->ring[consumer] + CACHE_LINE);
	*entry = q->ring[consumer];
	non_cc_store_int(&q->head, (consumer+1)%MSG_NUM);
	return 0;
}

static int __attribute__((optimize("O0")))
spin_delay(unsigned long long cycles)
{
        unsigned long long s, e, curr;
	int i=0;

        rdtscll(s);
        e = s + cycles;
        curr = s;
        while (curr < e) {
		rdtscll(curr);
		i++;
	}
	assert(curr >= e);
        return (int)(curr - e);
}

static int
rpc_send(int caller, int recv_node, int size, void *addr)
{
	int ret;
	struct msg_meta meta;

	spin_delay(DELAY);
	clwb_range_opt(addr, addr+size);
	meta.size = size;
	meta.off  = (int)((char *)addr-(char *)global_msg_pool);
	ret = msg_enqueue(&global_msg_pool->nodes[recv_node].recv[caller], &meta);
	return ret;
}

static void *
rpc_recv_ext(int caller, int rcv, int spin)
{
	int deq;
	struct msg_meta meta;
	void *addr;

	do {
		deq = msg_dequeue(&global_msg_pool->nodes[caller].recv[rcv], &meta);
		if (!deq) {
			addr = (char *)global_msg_pool + meta.off;
			clflush_range(addr, addr+meta.size);
			return addr;
		}
	} while(spin);
	return NULL;
}

static void *
rpc_recv(int caller, int spin)
{
	int deq, i;
	struct msg_meta meta;
	struct mem_meta * mem;
	void *addr;

	do {
		spin_delay(DELAY);
		for(i=(caller+1)%NUM_NODE; i!=caller; i = (i+1)%NUM_NODE) {
			addr = rpc_recv_ext(caller, i, 0);
			if (addr) return addr;
		}
	} while(spin);
	return NULL;
}

static void *
rpc_addr(int snd, int rcv, int idx)
{
	return msg_buf[rcv][snd][idx%2];
}

static void
rpc_sync(int num)
{
	cos_faa(&global_msg_pool->done, 1);
	while (*(volatile int *)&(global_msg_pool->done) != num) { ; }
}

static void *
rpc_init(void *addr, int node)
{
	int i, j;
	char *pos = (char *)addr;

	global_msg_pool = (struct msg_pool *)pos;
	pos += round_up_to_page(sizeof(struct msg_pool));
	for(i=0; i<NUM_NODE; i++) {
		for(j=0; j<NUM_NODE; j++) {
			msg_buf[i][j][0] = pos;
			pos += PAGE_SIZE;
			msg_buf[i][j][1] = pos;
			pos += PAGE_SIZE;
		}
	}
	if (!node) memset((void *)global_msg_pool, 0, sizeof(struct msg_pool));
	return pos;
}


#endif /* RPC_H */
