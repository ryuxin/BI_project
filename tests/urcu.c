#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>
#include <urcu.h>
#include "bi_rcu.h"
#include "args.h"

#define TEST_FILE_ADDR (NULL)
#define N_OPS (10000)
#define N_LOG (N_OPS)
#define TEST_THD_NUM 8

static char ops[N_OPS];
static unsigned long p99_log[N_LOG];

extern void rcu_bi_init(void);

static int
cmpfunc(const void * a, const void * b)
{ return ( *(int*)a - *(int*)b ); }

void 
spin_delay(uint64_t cycles)
{
	uint64_t s, e, curr;

	s = bi_local_rdtsc();
	e = s + cycles;
	curr = s;
	while (curr < e) curr = bi_local_rdtsc();
	return;
}

void *
bench(void *arg)
{
	int i;
	unsigned long n_read = 0, n_update = 0;
	uint64_t s, e, s1, e1, tot_cost_r = 0, tot_cost_w = 0, max = 0, cost;
	unsigned long r_99 = 0, w_99 = 0;
	int jump;

	jump = (int)(long)arg;
	thd_set_affinity(pthread_self(), 0, jump);
	bi_local_init_reader(jump);
	s = bi_local_rdtsc();
	for (i = jump; i < N_OPS; i+=(1+jump)) {
		s1 = bi_local_rdtsc();

		if (ops[i]) {
			synchronize_rcu();

			e1 = bi_local_rdtsc();
			cost = e1-s1;
			tot_cost_w += cost;
			n_update++;
			if (!jump) p99_log[N_LOG - n_update] = cost;
		} else {
			rcu_read_lock();
			rcu_read_unlock();

			e1 = bi_local_rdtsc();
			cost = e1-s1;
			tot_cost_r += cost;
			if (!jump) p99_log[n_read] = cost;
			n_read++;
		}

		if (cost > max) max = cost;
	}
	assert(n_read + n_update <= N_LOG);
	e = bi_local_rdtsc();

	if (n_read)   tot_cost_r /= n_read;
	if (n_update) tot_cost_w /= n_update;
	if (!jump && n_read) {
		qsort(p99_log, n_read, sizeof(unsigned long), cmpfunc);
		r_99 = p99_log[n_read - n_read / 100];
	}
	if (!jump && n_update) {
		qsort(&p99_log[n_read], n_update, sizeof(unsigned long), cmpfunc);
		w_99 = p99_log[N_LOG - 1 - n_update / 100];
	}
	if (!jump) printf("99p: read %lu write %lu\n", r_99, w_99);
	printf("thd %d tot %lu ops (r %lu, u %lu) done, %lu (r %lu, w %lu) cycles per op, max %lu\n",
		jump, n_read+n_update, n_read, n_update, (e-s)/(n_read + n_update),
		tot_cost_r, tot_cost_w, max);
	return NULL;
}

void
test_urcu(void)
{
	int ret, i;
	pthread_t pthds[TEST_THD_NUM];

	ret = mlockall(MCL_CURRENT | MCL_FUTURE);
	if (ret) {
		printf("cannot lock memory %d... exit.\n", ret);
		exit(-1);
	}
	load_trace(N_OPS, 50, ops);
	for(i=0; i<TEST_THD_NUM; i++) {
		ret = pthread_create(&pthds[i], 0, bench, (void *)(long)i);
		if (ret) {
			perror("pthread create of child\n");
			exit(-1);
		}
	}
	sleep(3);
	for(i=0; i<TEST_THD_NUM; i++) {
		pthread_join(pthds[i], NULL);
	}
	printf("BI urcu unit tests:  SUCCESS!\n");
	return;
}

int
main(int argc, char *argv[])
{
	void *mem;
	struct Mem_layout *layout;

	setup_core_id(0);
	test_parse_args(argc, argv);
	mem = bi_global_init_master(id_node, num_node, num_core,
				    TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR, 
				    "BI urcu tests");
	bi_rcu_init_global((struct RCU_block *)get_rcu_area());
	layout = (struct Mem_layout *)mem;
	printf("test: %s\n", layout->magic);
	assert(id_node == NODE_ID());
	bi_rcu_init_local();
	rcu_bi_init();

	test_urcu();

	return 0;
}

