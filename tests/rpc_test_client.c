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
#include "rpc_test_common.h"

#define ITER (1024)

static void
rpc_test_client(struct Mem_layout *layout)
{
	int test, r;
	size_t s, snt_sz;
	int id=0;

	snt_sz = sizeof(struct Rpc_test_msg);
	printf("rpc test client %s\n", layout->magic);
	while (1) {
		scanf("%d", &test);
		if (test == 0) {
			sprintf(test_snt_msg.message, "exit");
			r = rpc_send(0, &test_snt_msg, snt_sz);
			assert(r == 0);
			break;
		}

		sprintf(test_snt_msg.message, "rpc test send %d", id++);
		r = rpc_send(0, &test_snt_msg, snt_sz);
		assert(r == 0);

		s = rpc_recv(0, &test_rcv_msg, 1);
		assert(s == snt_sz);
		printf("recv %s\n", test_rcv_msg.message);
	}
}

static void
rpc_bench_client(size_t sz)
{
	int i, r;
	uint64_t start, end;
	size_t s;

	sprintf(test_snt_msg.message, "rpc test bench request");
	start = bi_local_rdtsc();
	for(i=0; i<ITER; i++) {
		r = rpc_send(0, &test_snt_msg, sz);
		assert(r == 0);
		s = rpc_recv(0, &test_rcv_msg, 1);
		assert(s == sz);
	}
	end = bi_local_rdtsc();
	sprintf(test_snt_msg.message, "exit");
	assert(end > start);
	rpc_send(0, &test_snt_msg, sz);
	printf("rpc round trip sz %lu avg cost %lu\n", sz,  (end - start)/ITER);
}

int
main(int argc, char *argv[])
{
	void *mem;
	struct Mem_layout *layout;

	test_parse_args(argc, argv);
	mem = bi_global_init_slave(id_node, num_node, num_core,
				   TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR);
	layout = (struct Mem_layout *)mem;

	if (test_case == 1) {
		non_cc_test(layout);
		rpc_test_client(layout);
	} else {
		rpc_bench_client(1);
		rpc_bench_client(128);
	}
	return 0;
}
