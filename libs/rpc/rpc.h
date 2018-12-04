#ifndef RPC_H
#define RPC_H

#define MAX_MSG_SIZE 256
#define MSG_NUM 16
//#define NO_HEAD 1

struct msg_meta {
	int size;   /* message size */
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
int rpc_send(int node, int core, void *data, int size);
/* receive a message from (node, core), return size and copy message to data */ 
int rpc_recv(int node, int core, void *data, int spin); 
void rpc_init_global();
void rpc_init_local();



/*********** FIXME ******************/
/* single producer single consumer queue */
#ifdef NO_HEAD
static inline int
msg_enqueue(struct msg_queue *q, int *pos, struct msg_meta *entry)
{
	struct msg_meta *m;
	int cur = *pos;

	m = &(q->ring[cur]);
	cos_flush_cache(m);
	if (m->use) return -1;
	m->mem_id = entry->mem_id;
	m->size   = entry->size;
	m->use    = 1;
	*pos      = (cur+1) & (MSG_NUM-1);
	cos_wb_cache(m);
	return 0;
}

static inline int
msg_dequeue(struct msg_queue *q, int *pos, struct msg_meta *entry)
{
	struct msg_meta *m;
	int cur = *pos;

	m = &(q->ring[cur]);
	cos_flush_cache(m);
	if (!m->use) return -1;
	entry->mem_id = m->mem_id;
	entry->size   = m->size;
	m->use        = 0;
	*pos          = (cur+1) & (MSG_NUM-1);
	cos_wb_cache(m);
	return 0;
}
#else
static inline int
msg_enqueue(struct msg_queue *q, struct msg_meta *entry)
{
	int consumer, producer, delta;

	consumer = non_cc_load_int(&q->head);
	producer = q->tail;
	delta = (producer + 1)%MSG_NUM;
	if (delta == consumer) return -1;
	q->ring[producer] = *entry;
	cos_wb_cache(&q->ring[producer]);
	non_cc_store_int(&q->tail, delta);
	return 0;
}

static inline int
msg_dequeue(struct msg_queue *q, struct msg_meta *entry)
{
	int consumer, producer;

	consumer = q->head;
	producer = non_cc_load_int(&q->tail);
	if (consumer == producer) return -1;
	cos_flush_cache(&q->ring[consumer]);
	*entry = q->ring[consumer];
	non_cc_store_int(&q->head, (consumer+1)%MSG_NUM);
	return 0;
}
#endif

#endif /* RPC_H */
