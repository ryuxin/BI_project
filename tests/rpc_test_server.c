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

static void
rpc_test_server(struct Mem_layout *layout)
{
	int r;
	size_t s;
	int id=0, nd, cd;

	printf("rpc test server %s\n", layout->magic);
	while (1) {
		s = rpc_recv_server(&test_rcv_msg, &nd, &cd);
		if (!s) continue;
		assert(nd == 1);
		assert(cd == 0);
		printf("recv %s\n", test_rcv_msg.message);
		if (test_rcv_msg.message[0] == 'e') break;

		sprintf(test_snt_msg.message, "rpc test reply %d", id++);
		r = rpc_send_server(nd, cd, &test_snt_msg, s);
		assert(r == 0);
	}
}

static void
rpc_bench_server(size_t sz)
{
	int r;
        size_t s;
	int nd, cd;

	sprintf(test_snt_msg.message, "rpc test bench reply");
	while (1) {
		s = rpc_recv_server(&test_rcv_msg, &nd, &cd);
		if (!s) continue;
		assert(s == sz);
		if (test_rcv_msg.message[0] == 'e') break;
		r = rpc_send_server(nd, cd, &test_snt_msg, sz);
		assert(r == 0);
	}
}

int
main(int argc, char *argv[])
{
	void *mem;
	struct Mem_layout *layout;

	test_parse_args(argc, argv);
	mem = bi_global_init_master(id_node, num_node, num_core,
				    TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR, 
				    "rpc simple tests");
	layout = (struct Mem_layout *)mem;

	if (test_case == 1) {
		non_cc_test(layout);
		rpc_test_server(layout);
	} else {
		rpc_bench_server(1);
		rpc_bench_server(128);
	}
	return 0;
}
