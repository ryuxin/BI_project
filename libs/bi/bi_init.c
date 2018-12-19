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

/************ TODO: add node local init wrter update fucntion, mem alloc, core id ***********/
static void *
__global_init_share(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr)
{
	int fd;
	void *mem;

	setup_node_id(node_id);
	setup_node_num(node_num);
	setup_core_num(core_num);

	fd = open(test_file, O_CREAT | O_RDWR, 0666);
	ftruncate(fd, file_size);
	mem = mmap(map_addr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED) {
		printf("mmap failed: file %s sz %ld\n", test_file, file_size);
		exit(-1);
	}
	rpc_init_global();

	return mem;
}

void *
bi_global_init_master(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr, char *test_string)
{
	void *mem, *end;

	mem = __global_init_share(node_id, node_num, core_num, test_file, file_size, map_addr);
	// memset(mem, 0, file_size);
	end = init_global_memory(mem, test_string);
	assert(end - mem < file_size);
	bi_global_rtdsc();
	clwb_range(mem, file_size);

	return mem;
}

void *
bi_global_init_slave(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr)
{
	void *mem;

	mem = __global_init_share(node_id, node_num, core_num, test_file, file_size, map_addr);
	clflush_range(mem, file_size);

	return mem;
}

void
bi_local_init_reader(int core_id)
{
	setup_core_id(core_id);
}
