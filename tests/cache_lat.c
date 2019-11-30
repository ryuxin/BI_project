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
#include <pthread.h>
#define _GNU_SOURCE
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "bi.h"
#include "args.h"

#define TEST_FILE_ADDR ((void *)0x7f5fb75f4000)
#define PAGE_NUM (256)
#define ITER (1024)

typedef enum {
	WARMUP,
	READ,
	WRITE,
	WRITEBACK,
	FLUSH,
	DONE
} ops_t;

struct test_op {
	ops_t op;
};

struct test_op test_msg;
char *test_start_addr;

static inline void
execute_op_cache(ops_t op, char *addr)
{
	char t = '$';
	int i;

	switch (op) {
	case WARMUP:
	case READ:
		for(i=0; i<CACHE_LINE; i++) t = addr[i];
		break;
	case WRITE:
		for(i=0; i<CACHE_LINE; i++) addr[i] = t;
		break;
	case FLUSH:
		clflush_range(addr, CACHE_LINE);
		break;
	case WRITEBACK:
		clwb_range(addr, CACHE_LINE);
		break;
	default:
		printf("BUG BUG op %d\n", op);
	}
}

static inline uint64_t
execute_op_bench(ops_t op)
{
	uint64_t start, end;
	int i;

	start = bi_local_rdtsc();
	for(i=0; i<PAGE_NUM; i++) {
		execute_op_cache(op, test_start_addr + i*PAGE_SIZE);
	}
	end = bi_local_rdtsc();
	return (end - start) / PAGE_NUM;
}

static inline uint64_t
execute_test(ops_t local, ops_t remote)
{
	int r;
        size_t s;

	test_msg.op = remote;
	r = rpc_send(1, &test_msg, sizeof(struct test_op));
	assert(r == 0);
	s = rpc_recv(1, &test_msg, 1);
	assert(s == sizeof(struct test_op));
	assert(test_msg.op == DONE);
	return execute_op_bench(local);
}

static void
cache_lat_test(char *name, ops_t local, ops_t remote)
{
	int i;
	uint64_t tot = 0;

	execute_test(WARMUP, WARMUP);
	for(i=0; i<ITER; i++) {
		tot += execute_test(local, remote);
	}
	printf("%15s\t%lu\n", name, tot/ITER);
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
		execute_op_bench(test_msg.op);
		test_msg.op = DONE;
		r = rpc_send_server(nd, cd, &test_msg, sizeof(struct test_op));
		assert(r == 0);
	}
}

int main(int argc, char *argv[])
{
	void *mem;
	struct Mem_layout *layout;

	test_parse_args(argc, argv);
	thd_set_affinity(pthread_self(), id_node, 0);
	if (!id_node) {
		mem = bi_global_init_master(id_node, num_node, num_core,
					    TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR, 
    					    "cache latency  benchmark");
	} else {
		mem = bi_global_init_slave(id_node, num_node, num_core,
					   TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR);
		mem_mgr_init();
	}
	layout = (struct Mem_layout *)mem;
	test_start_addr = get_malloc_start_addr(0);
	printf("start cache latency benchmark at %p\n", test_start_addr);
	if (!id_node) {
		cache_lat_test("read(r)", READ, READ);
	} else {
		test_server();
	}
	return 0;
}
