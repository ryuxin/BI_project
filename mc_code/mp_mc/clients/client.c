#include "../share/constant.h"
#include "../share/rpc.h"
#include "../share/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "hash.h"

#define N_OPS                 10000000
#define N_KEYS                4000000
/*#define N_KEYS                40*/

void *map_addr;
char ops[N_OPS][KEY_LENGTH + 1];
int cur_node, req_cost[N_OPS];
int n_read, n_update, n_tot, nget;

static int cmpfunc(const void * a, const void * b)
{
	return ( *(int*)b - *(int*)a );
}

static void
out_latency(void)
{
	qsort(req_cost, n_tot, sizeof(int), cmpfunc);
	printf("tot %d %d 99.9 %d 99 %d min %d max %d\n", n_tot, n_read+n_update, 
		req_cost[n_tot/1000], req_cost[n_tot/100], req_cost[n_tot-1], req_cost[0]);
}

static inline void
disconnect_mc_server(int me, int server, mc_message_t type)
{
	struct mc_msg *msg;

	msg       = (struct mc_msg *)rpc_addr(me, server);
	msg->type = type;
	msg->id   = me;
	rpc_send(msg->id, server, sizeof(struct mc_msg), msg);
}

static int client_get_key(char *key, int nkey)
{
	void *rcv;
	char *data;
	int node, r, msg_sz;
	uint32_t hv;
	struct mc_msg *msg;

	hv           = hash(key, nkey);
	node         = hv2node(hv);
	msg          = (struct mc_msg *)rpc_addr(cur_node, node);
	msg_sz       = sizeof(struct mc_msg)+nkey;
	msg->type    = MC_MSG_GET;
	msg->id      = cur_node;
	msg->hv      = hv;
	msg->nkey    = nkey;
	msg->key     = (char *)&msg[1];
	msg->key_off = msg->key-(char *)msg;
	memcpy(msg->key, key, nkey);
	assert(msg_sz < PAGE_SIZE);

	r = rpc_send(msg->id, node, msg_sz, msg);
	assert(!r);
	msg = (struct mc_msg *)rpc_recv(msg->id, 1);
	assert(msg->id == node);

	if (msg->type == MC_MSG_GET_OK) {
		assert(msg->nbytes == V_LENGTH);
		assert(msg->data);
		data = (char *)msg + msg->data_off;
		assert(data[0] == '$');
		r = 0;
	} else if (msg->type == MC_MSG_GET_FAIL) {
		assert(msg->nbytes == 0);
		assert(!msg->data);
		r = -1;
	} else {
		assert(0);
	}
	return r;
}

static int client_set_key(char *key, int nkey, char *data, int nbytes)
{
	void *rcv;
	int node, r, msg_sz;
	uint32_t hv;
	struct mc_msg *msg;

	hv            = hash(key, KEY_LENGTH);
	node          = hv2node(hv);
	msg           = (struct mc_msg *)rpc_addr(cur_node, node);
	msg_sz        = sizeof(struct mc_msg)+nkey+nbytes;
	msg->type     = MC_MSG_SET;
	msg->id       = cur_node;
	msg->hv       = hv;
	msg->nkey     = nkey;
	msg->key      = (char *)&msg[1];
	msg->key_off  = msg->key-(char *)msg;
	msg->nbytes   = nbytes;
	msg->data     = msg->key+nkey;
	msg->data_off = msg->data-(char *)msg;
	memcpy(msg->key, key, nkey);
	memcpy(msg->data, data, nbytes);
	assert(msg_sz < PAGE_SIZE);

	r = rpc_send(msg->id, node, msg_sz, msg);
	assert(!r);
	msg = (struct mc_msg *)rpc_recv(msg->id, 1);
	assert(msg);
	assert(msg->id == node);
#ifdef KEY_SKEW
	if (msg->type == MC_MSG_SET_OK) return 0;
	else if (msg->type == MC_MSG_SET_FAIL) return -1;
	else assert(0);
#else
	assert(msg->type == MC_MSG_SET_OK);
#endif
	return 0;
}

static void bench(void)
{
	int i, ret;
	char *op, value[V_LENGTH], *key;
	unsigned long long s, e, s1, e1, cost;
	unsigned long long tot_cost = 0, tot_r, tot_w;
	unsigned long long prev = 0;

	/* prepare the value -- no real database op needed. */
	memset(value, '$', V_LENGTH);
	n_read = n_update = n_tot = nget = 0;
	tot_r = tot_w = 0;

	rdtscll(s);
        for (i = cur_node - (NUM_NODE/2); i < N_OPS; i+=(NUM_NODE/2)) {
	/* for (i = 0; i < N_OPS; i++) { */
		op = ops[i];
		key = &op[1];

                if (*op == 'R') {
                        rdtscll(s1);
                        n_read++;
                        ret = client_get_key(key, KEY_LENGTH);
                        rdtscll(e1);
                        cost = e1-s1;
                        assert(e1 > s1);
                        tot_r += cost;
                } else {
                        rdtscll(s1);
                        assert(*op == 'U');
                        n_update++;
                        ret = client_set_key(key, KEY_LENGTH, value, V_LENGTH);
                        assert(ret == 0);
                        rdtscll(e1);
                        cost = e1-s1;
                        assert(e1 > s1);
                        tot_w += cost;
                }
                tot_cost += cost;
                if (!ret) {
                        if (*op == 'R') nget++;
                        req_cost[n_tot++] = (int)cost;
                }
	}
	rdtscll(e);
	assert(e > s);

	printf("Node %d: tot %d ops (r %d, u %d) done, time(ms) %llu, thput %llu\n",
	       cur_node, n_read+n_update, n_read, n_update, (e - s)/(unsigned long long)CPU_FREQ, 
	       (unsigned long long)CPU_FREQ * n_tot * 1000 / (e - s));
	unsigned long long rl, ul;
	if (n_read == 0) rl = 0;
	else rl = tot_r/n_read;
	if (n_update == 0) ul = 0;
	else ul = tot_w/n_update;
        printf("%llu (%llu) op, get %llu, set %llu miss %d (%%%%) %d\n", (unsigned long long)(e-s)/(n_read + n_update),
	       tot_cost/(n_read+n_update), rl, ul, n_read - nget, (n_read ? (n_read - nget) * 1000 / n_read : 0));
}

void preload_key(void)
{
	/* insert all the keys into the cache before accessing the
	* traces. If the cache is large enough, there will be no miss.*/
	char buf[KEY_LENGTH + 1], v[V_LENGTH];
	int bytes = KEY_LENGTH + 1, i, fd, r;
	char *load_file = "./preload_key";
	unsigned long long start, end;
	uint32_t hv;

	fd = open(load_file, O_RDONLY);
	if (fd < 0) {
		printf("cannot open file %s. Exit.\n", load_file);
		exit(-1);
	}

	rdtscll(start);
	for (i = 0; i < N_KEYS; i++) {
		bytes = read(fd, buf, KEY_LENGTH + 1);
		assert(bytes == KEY_LENGTH + 1);
		memset(v, '$', V_LENGTH);
		r = client_set_key(buf, KEY_LENGTH, v, V_LENGTH);
		if (r) break;
	}
	rdtscll(end);
	assert(end > start);
	close(fd);
	printf("load key node finish total key %d avg %llu\n", i, (end-start)/(unsigned long long)i);
        disconnect_mc_server(cur_node, 0, MC_BEGIN);
        disconnect_mc_server(cur_node, 1, MC_BEGIN);
}

void load_trace(void)
{
	int fd, bytes, i, id = cur_node - NUM_NODE/2;
	char *load_file = "./trace_key", buf[KEY_LENGTH + 2];

	/* read the entire trace into memory. */
	fd = open(load_file, O_RDONLY);
	if (fd < 0) {
		printf("cannot open file %s. Exit.\n", load_file);
		exit(-1);
	}

/* #define SI(x)   ((x)*(N_OPS - 1)) */
/* #define STEP(x) (-2*(x) + 1) */
/* #define EI(x)   (SI(1-x) + STEP(x)) */

#define SI(x)   (0)
#define STEP(x) (1)
#define EI(x)   (N_OPS)

	for(i = SI(id); i != EI(id); i += STEP(id)) {
		bytes = read(fd, buf, KEY_LENGTH+2);
		assert(bytes == KEY_LENGTH + 2);
		assert(buf[KEY_LENGTH + 1] == '\n');
		memcpy(ops[i], buf, KEY_LENGTH + 1);
	}
	close(fd);
}

inline void client_init(void)
{
	if(cur_node == NUM_NODE/2) preload_key();
	load_trace();
}

inline void *client_start(void)
{
	rpc_sync(NUM_NODE/2);
	bench();
	rpc_sync(NUM_NODE);
/*	out_latency();*/
	disconnect_mc_server(cur_node, cur_node-NUM_NODE/2, MC_EXIT);
	return NULL;
}

int main(int argc, char *argv[])
{
	int fd;

	if (argc != 2) {
		printf("usage: %s node_id\n", argv[0]);
		exit(-1);
	}
	cur_node = atoi(argv[1]);
	if (cur_node < NUM_NODE/2) {
		printf("client id %d should be larger than %d\n", cur_node, NUM_NODE/2);
		exit(-1);
	}
	fd = open(MAPPING_FILE, O_RDWR);
	if (fd < 0) {
		printf("cannot open testing file %s. Exit.\n", MAPPING_FILE);
		exit(-1);
	}
	map_addr = mmap(0, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map_addr == MAP_FAILED) {
		printf("cannot map testing file %s. %s. Exit.\n", MAPPING_FILE, strerror(errno));
		close(fd);
		exit(-1);
	}
	rpc_init(map_addr, cur_node);
	hash_init(JENKINS_HASH);
//	set_prio();
	client_init();
	client_start();

	return 0;
}
