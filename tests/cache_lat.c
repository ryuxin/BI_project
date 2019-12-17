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
#include <time.h> 
#include <pthread.h>
#define _GNU_SOURCE
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "bi.h"
#include "args.h"

#define TEST_FILE_ADDR ((void *)0x7f5e775f400)
#define PAGE_NUM (256)
#define ITER (1024)

typedef enum {
	WARMUP,
	READ,
	WRITE,
	WRITEBACK,
	FLUSH,
	EMPTY,
	DONE
} ops_t;

struct test_op {
	ops_t op;
	int num;
};

FILE *gf;
struct test_op test_msg;
char *test_start_addr;
static volatile uint64_t use_result_dummy = 0;
int gnum = 0;

void
use_int(int result) { use_result_dummy += result; }

static void
my_shuffle(int *arr, int sz)
{
	int i, r, t;
	for(i=sz-1; i>0; i--) {
		r = rand() % i;
		t = arr[i];
		arr[i] = arr[r];
		arr[r] = t;
	}
}

static inline void
execute_op_cache(const ops_t op, void *addr)
{
	register int s;
	register int *p;
	register ops_t lop;

	p = addr;
	s = 0;
	lop = op;
	switch (op) {
	case WARMUP:
	case READ:
		s += 
#undef	DOIT
#define	DOIT(i)	p[i]+
//		DOIT(0) DOIT(4) DOIT(8) DOIT(12) DOIT(16) DOIT(20) DOIT(24) DOIT(28) 
//		DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52) DOIT(56) p[60];
		p[0];
		break;
	case WRITE:
#undef	DOIT
#define	DOIT(i)	p[i] = 1;
		DOIT(0)
//		DOIT(0) DOIT(4) DOIT(8) DOIT(12) DOIT(16) DOIT(20) DOIT(24) DOIT(28) 
//		DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52) DOIT(56) DOIT(60)
		break;
	case FLUSH:
		clflush_range(addr, 2*CACHE_LINE);
		break;
	case WRITEBACK:
		clwb_range(addr, 2*CACHE_LINE);
		break;
	case EMPTY:
		break;
	default:
		printf("BUG BUG op %d\n", op);
	}
	use_int(s);
}

static inline uint64_t
execute_op_bench(ops_t op)
{
	uint64_t start, end, tot;
	int i, rnd[PAGE_NUM];

	for(i=0; i<PAGE_NUM; i++) rnd[i] = i;
	my_shuffle(rnd, PAGE_NUM);
	start = bi_local_rdtsc();
	for(i=0; i<PAGE_NUM; i++) {
		execute_op_cache(EMPTY, test_start_addr + i*PAGE_SIZE);
	}
	end = bi_local_rdtsc();
	assert(end > start);
	tot = end - start;
//	for(i=0; i<PAGE_NUM; i++) {
//		execute_op_cache(FLUSH, test_start_addr + rnd[i]*PAGE_SIZE);
//	}
	bi_mb();
	bi_ccb();
	start = bi_local_rdtsc();
	for(i=0; i<PAGE_NUM; i++) {
		execute_op_cache(op, test_start_addr + rnd[i]*PAGE_SIZE);
	}
	end = bi_local_rdtsc();
	assert(end > start);
	fprintf(gf, "dbg op %d tot %lu time %lu act %lu\n", op, tot, end-start, end-start-tot);
	//return (end - start) / PAGE_NUM;
	return (end - start - tot) / PAGE_NUM;
}

static inline uint64_t
execute_test(ops_t local, ops_t remote)
{
	int r;
        size_t s;

	test_msg.op = remote;
	test_msg.num = gnum++;
	r = rpc_send(1, &test_msg, sizeof(struct test_op));
	assert(r == 0);
	s = rpc_recv(1, &test_msg, 1);
	assert(s == sizeof(struct test_op));
	assert(test_msg.op == DONE);
	assert(test_msg.num == gnum - 1);
	return execute_op_bench(local);
}

static void
cache_lat_test(char *name, ops_t local, ops_t remote)
{
	int i;
	uint64_t rt, tot = 0, ma = 0;

	execute_test(WARMUP, WARMUP);
	execute_test(local, remote);
	for(i=0; i<ITER; i++) {
		rt = execute_test(local, remote);
		if (rt > 2000) {
			i--; // ignore abnormal case inerrupt, etc...
			continue;
		}
		tot += rt;
		if (rt>ma) ma = rt;
	}
	printf("%15s\t%lu\t%lu\n", name, tot/ITER, ma);
}

static void
test_server()
{
	int r;
        size_t s;
	int nd, cd;

	while (1) {
		s = rpc_recv_server(&test_msg, &nd, &cd);
		if (!s) continue;
		assert(nd == 0);
		assert(cd == 0);
		assert(s  == sizeof(struct test_op));
		assert(test_msg.num == gnum);
		execute_op_bench(test_msg.op);
		bi_mb();
		bi_ccb();
		test_msg.op = DONE;
		test_msg.num = gnum++;
		r = rpc_send_server(nd, cd, &test_msg, sizeof(struct test_op));
		assert(r == 0);
	}
}

int main(int argc, char *argv[])
{
	void *mem;
	struct Mem_layout *layout;

	srand(time(0));
	gf = fopen("/tmp/lat", "w");
	test_parse_args(argc, argv);
	thd_set_affinity(pthread_self(), id_node, 0);
	bi_local_init_server(0, 1);
	if (!id_node) {
		mem = bi_global_init_master(id_node, num_node, num_core,
					    TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR, 
    					    "cache latency benchmark");
	} else {
		mem = bi_global_init_slave(id_node, num_node, num_core,
					   TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR);
		mem_mgr_init();
	}
	layout = (struct Mem_layout *)mem;
	printf("magic: %s\n", layout->magic);
	test_start_addr = get_malloc_start_addr(0);
	printf("start cache latency benchmark at %p\n", test_start_addr);
	if (!id_node) {
		cache_lat_test("read(l)", READ, EMPTY);
		cache_lat_test("read(r)", READ, READ);
		cache_lat_test("read(m)", READ, WRITE);
		cache_lat_test("read(i)", READ, FLUSH);
		cache_lat_test("write(l)", WRITE, EMPTY);
		cache_lat_test("write(r)", WRITE, READ);
		cache_lat_test("write(m)", WRITE, WRITE);
		cache_lat_test("write(i)", WRITE, FLUSH);
	} else {
		test_server();
	}
	fclose(gf);
	return 0;
}
