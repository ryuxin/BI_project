#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "args.h"

#define TEST_FILE_ADDR (NULL)
#define SMALLSZ 1
#define LARGESZ 8000
#define ITER       (1024)
#define SMALLCHUNK 32
#define LARGECHUNK 32

struct small {
	char x[SMALLSZ];
};

struct larger {
	char x[LARGESZ];
};

struct small  *s[ITER];
struct larger *l[ITER];
uint64_t free_tsc, alloc_tsc;
unsigned long cost[ITER], alloc[ITER];
struct ps_slab_info *l_slab, *s_slab;

void
mark(char *c, int sz, char val)
{
	int i;

	for (i = 0 ; i < sz ; i++) c[i] = val;
}

void
chk(char *c, int sz, char val)
{
	int i;

	for (i = 0 ; i < sz ; i++) assert(c[i] == val);
}

static inline int
cmpfunc(const void * a, const void * b)
{ return (*(unsigned long*)b) - (*(unsigned long*)a); }

static inline void
out_latency(unsigned long *re, int num, char *label)
{
	int i;
	uint64_t sum = 0;

	for (i = 0; i < num; i++) sum += (uint64_t)re[i];
	qsort(re, num, sizeof(unsigned long), cmpfunc);
	printf("%s tot %d avg %lu 99.9 %lu 99 %lu min %lu max %lu\n", label,
	       num, sum/num, re[num/1000], re[num/100], re[num-1], re[0]);
}

void
mt_consumer(struct ps_slab_info *sl, void **rb, char *name)
{
	char *s;
	long i;
	uint64_t start, end;
	(void)sl;

	for (i = 0; i < ITER; i++) {
		s = (char *)rb[i];

		rb[i] = NULL;
		assert(i == ((int *)s)[0]);

		start = bi_local_rdtsc();
		bi_slab_free(s);
		end = bi_local_rdtsc();
		assert(end > start);
		cost[i] = end-start;
	}
	out_latency(cost, i, name);
}

void
mt_producer(struct ps_slab_info *sl, void **rb, char *name)
{
	void *s;
	unsigned long i;
	uint64_t start, end;

	for (i = 0; i < ITER; i++) {
		start = bi_local_rdtsc();
		s = bi_slab_alloc(sl);
		end = bi_local_rdtsc();
		assert(s);
		assert(end > start);

		((int *)s)[0] = i;
		rb[i] = s;
		alloc[i] = end-start;
	}
	out_latency(alloc, i, name);
}

void
test_producer_comsumer(void)
{
	mt_producer(s_slab, (void **)s, "small obj alloc");
	mt_consumer(s_slab, (void **)s, "small obj free");

	mt_producer(l_slab, (void **)l, "large obj alloc");
	mt_consumer(l_slab, (void **)l, "large obj free");
}

void
test_correctness(void)
{
	int i, j;

	for (i = 0 ; i < ITER ; i+=4) {
		l[i] = bi_slab_alloc(l_slab);
		assert(l[i]);
		mark(l[i]->x, sizeof(struct larger), i);
		for (j = i+1 ; j < ITER ; j++) {
			l[j] = bi_slab_alloc(l_slab);
			assert(l[j]);
			mark(l[j]->x, sizeof(struct larger), j);
		}
		for (j = i+1 ; j < ITER ; j++) {
			chk(l[j]->x, sizeof(struct larger), j);
			bi_slab_free(l[j]);
		}
	}
	for (i = 0 ; i < ITER ; i+=4) {
		assert(l[i]);
		chk(l[i]->x, sizeof(struct larger), i);
		bi_slab_free(l[i]);
	}
	printf("correctness test passed!\n");
}

void
test_perf(void)
{
	int i, j;
	uint64_t start, end;

	printf("Slabs:\n"
	       "\tsmall: objsz %lu, objmem %lu, nobj %lu\n"
	       "\tlarge: objsz %lu, objmem %lu, nobj %lu\n",
	       (unsigned long)sizeof(struct small),  (unsigned long)bi_slab_objmem(s_slab), (unsigned long)bi_slab_nobjs(s_slab),
	       (unsigned long)sizeof(struct larger), (unsigned long)bi_slab_objmem(l_slab), (unsigned long)bi_slab_nobjs(l_slab));

	start = bi_local_rdtsc();
	for (j = 0 ; j < ITER ; j++) {
		for (i = 0 ; i < LARGECHUNK ; i++) l[i] = bi_slab_alloc(l_slab);
		for (i = 0 ; i < LARGECHUNK ; i++) bi_slab_free(l[i]);
	}
	end = bi_local_rdtsc();
	assert(end > start);
	end = (end-start)/(ITER*LARGECHUNK);
	printf("Average cost of large slab alloc+free: %lu\n", end);

	start = bi_local_rdtsc();
	for (j = 0 ; j < ITER ; j++) {
		for (i = 0 ; i < SMALLCHUNK ; i++) s[i] = bi_slab_alloc(s_slab);
		for (i = 0 ; i < SMALLCHUNK ; i++) bi_slab_free(s[i]);
	}
	end = bi_local_rdtsc();
	assert(end > start);
	end = (end-start)/(ITER*SMALLCHUNK);
	printf("Average cost of small slab alloc+free: %lu\n", end);
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
	s_slab = bi_slab_create(sizeof(struct small));
	l_slab = bi_slab_create(sizeof(struct larger));

	test_correctness();
	test_perf();
	test_producer_comsumer();
	printf("BI slab unit tests:  SUCCESS!\n");

	return 0;
}
