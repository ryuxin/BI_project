#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>
#include "args.h"

#define TEST_FILE_ADDR (NULL)
#define TST_SZ 100
#define BENCH_SZ 1
#define ITER 1000
#define OUT_ITER 50
#define N_OPS (1800)
#define N_LOG (N_OPS)

static void *ptrs[ITER];
struct ps_slab_info *tst_slab, *bench_slab;
static char ops[N_OPS];
static unsigned long p99_log[N_LOG];

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

void
reclaim_all(int num)
{
	int r;
	spin_delay(QUISE_FLUSH_PERIOD);
	r = bi_smr_reclaim();
	assert(r == num);
}

int
bench(void)
{
	int i;
	unsigned long n_read = 0, n_update = 0;
	uint64_t s, e, s1, e1, tot_cost_r = 0, tot_cost_w = 0, max = 0, cost;
	unsigned long r_99 = 0, w_99 = 0;

	s = bi_local_rdtsc();
	for (i = 0 ; i < N_OPS; i++) {
		s1 = bi_local_rdtsc();

		if (ops[i]) {
			bi_smr_free(bi_slab_alloc(bench_slab));

			e1 = bi_local_rdtsc();
			cost = e1-s1;
			tot_cost_w += cost;
			n_update++;
			p99_log[N_LOG - n_update] = cost;
		} else {
			bi_enter();
			bi_exit();

			e1 = bi_local_rdtsc();
			cost = e1-s1;
			tot_cost_r += cost;
			p99_log[n_read] = cost;
			n_read++;
		}

		if (cost > max) max = cost;
	}
	assert(n_read + n_update <= N_LOG);
	e = bi_local_rdtsc();

	if (n_read)   tot_cost_r /= n_read;
	if (n_update) tot_cost_w /= n_update;
	if (n_read) {
		qsort(p99_log, n_read, sizeof(unsigned long), cmpfunc);
		r_99 = p99_log[n_read - n_read / 100];
	}
	if (n_update) {
		qsort(&p99_log[n_read], n_update, sizeof(unsigned long), cmpfunc);
		w_99 = p99_log[N_LOG - 1 - n_update / 100];
	}
	printf("99p: read %lu write %lu\n", r_99, w_99);
	printf("tot %lu ops (r %lu, u %lu) done, %lu (r %lu, w %lu) cycles per op, max %lu\n",
		n_read+n_update, n_read, n_update, (e-s)/(n_read + n_update),
		tot_cost_r, tot_cost_w, max);
	return (int)n_update;
}

void
test_mem(void)
{
	uint64_t start, end;
	int i, j;

	start = bi_local_rdtsc();
	for (j = 0 ; j < ITER ; j++) ptrs[j] = bi_slab_alloc(tst_slab);
	for (j = 0 ; j < ITER ; j++) bi_smr_free(ptrs[j]);
	end = bi_local_rdtsc();
	end = (end-start)/ITER;
	printf("Average cost of alloc->free: %lu\n", end);
	reclaim_all(ITER);
/*
	start = bi_local_rdtsc();
	for (j = 0 ; j < OUT_ITER ; j++) {
		for (i = 0 ; i < ITER ; i++) ptrs[i] = bi_slab_alloc(tst_slab);
		for (i = 0 ; i < ITER ; i++) bi_smr_free(ptrs[i]);
	}
	end = bi_local_rdtsc();
	end = (end-start)/(OUT_ITER*ITER);
	printf("Average cost of %d * (alloc->free): %lu\n", OUT_ITER, end);
	reclaim_all(OUT_ITER*ITER);
	printf("Starting complicated allocation pattern for increasing numbers of allocations.\n");
*/
	for (i = 0 ; i < OUT_ITER ; i++) {
		ptrs[i] = bi_slab_alloc(tst_slab);
		for (j = i+1 ; j < ITER ; j++) {
			ptrs[j] = bi_slab_alloc(tst_slab);
		}
		for (j = i+1 ; j < ITER ; j++) {
			bi_smr_free(ptrs[j]);
		}
		reclaim_all(ITER-1-i);
	}
	for (i = 0 ; i < OUT_ITER ; i++) {
		assert(ptrs[i]);
		bi_smr_free(ptrs[i]);
	}
	reclaim_all(OUT_ITER);
	printf("BI snr alloc test: SUCCESS\n");
}

void
test_bi(void)
{
	int ret;

	ret = mlockall(MCL_CURRENT | MCL_FUTURE);
	if (ret) {
		printf("cannot lock memory %d... exit.\n", ret);
		exit(-1);
	}
	load_trace(N_OPS, 50, ops);
	ret = bench();
	reclaim_all(ret);
	return;
}

void
test_quies(void)
{
	int i;
	int tot_cpu, tot_core;
	uint64_t start, end, tsc;

	tot_cpu  = get_active_node_num();
	tot_core = get_active_core_num();
	assert(tot_cpu == num_node);
	assert(tot_core == num_core);

	for(i=0; i<ITER; i++) {
		tsc = bi_quiesce();
		assert(tsc);
		assert(tsc < bi_global_rtdsc());
	}

	start = bi_local_rdtsc();
	for(i=0; i<ITER; i++) {
		bi_quiesce();
	}
	end = bi_local_rdtsc();
	end = (end - start)/ITER;
	printf("quies ncpu %d ncore %d avg %lu\n", tot_cpu, tot_core, end);
}

void
test_smr(void)
{
	unsigned int nums[] = {1, 1000};
	unsigned int i, r, j, k;
	uint64_t start, end, tot;

	for (i = 0 ; i < sizeof(nums)/sizeof(int) ; i++) {
		tot = 0;
		for(j=0; j<ITER; j++) {
			for(k=0; k<nums[i]; k++) {
				bi_smr_free(bi_slab_alloc(bench_slab));
			}
			spin_delay(QUISE_FLUSH_PERIOD);
			start = bi_local_rdtsc();
			r = bi_smr_reclaim();
			end = bi_local_rdtsc();
			assert(r == nums[i]);
			tot += (end - start);
		}
		printf("bi smr avg cost %lu of batch %d, per obj %lu\n", tot/ITER, nums[i], tot/ITER/nums[i]);
	}
}

void
test_flush(void)
{
	unsigned int nums[] = {1, 1000};
	unsigned int i, r, j, k;
	uint64_t start, end, tot;

	for (i = 0 ; i < sizeof(nums)/sizeof(int) ; i++) {
		tot = 0;
		for(j=0; j<ITER; j++) {
			for(k=0; k<nums[i]; k++) {
				bi_smr_free(bi_slab_alloc(bench_slab));
			}
			start = bi_local_rdtsc();
			r = bi_smr_flush();
			end = bi_local_rdtsc();
			tot += (end - start);
			assert(r == nums[i]);
			reclaim_all(nums[i]);
		}
		printf("bi flush sz %d avg cost %lu of batch %d, per obj %lu\n", BENCH_SZ, tot/ITER, nums[i], tot/ITER/nums[i]);
	}
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
				    "slab allocator tests");
	layout = (struct Mem_layout *)mem;
	printf("test: %s\n", layout->magic);
	assert(id_node == NODE_ID());
	tst_slab   = bi_slab_create(TST_SZ);
	bench_slab = bi_slab_create(BENCH_SZ);

	printf("Testing memory management functionalities.\n");
 	test_mem();
	printf("Testing quiescence detection.\n");
	test_quies();
	printf("Testing Scalable Memory Reclamation.\n");
	test_smr();
	printf("Testing bi runtime.\n");
	test_bi();
	printf("Testing bi flush.\n");
	test_flush();

	return 0;
}

