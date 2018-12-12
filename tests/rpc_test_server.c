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
	int id=0;

	printf("rpc test server %s\n", layout->magic);
	while (1) {
		s = rpc_recv(1, &test_rcv_msg, 1);
		assert(s);
		printf("recv %s\n", test_rcv_msg.message);
		if (test_rcv_msg.message[0] == 'e') break;

		sprintf(test_snt_msg.message, "rpc test reply %d", id++);
		r = rpc_send(1, &test_snt_msg, s);
		assert(r == 0);
	}
}

int
main(int argc, char *argv[])
{
	void *mem;
	struct Mem_layout *layout;

	test_parse_args(argc, argv);
	mem = bi_global_init_master(num_node, id_node, 
								TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR, 
								"rpc simple tests");
	layout = (struct Mem_layout *)mem;

	if (test_case == 1) {
		non_cc_test(layout);
		rpc_test_server(layout);
	} else {
		return 0;
	}
	return 0;
}
