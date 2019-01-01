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
#include "bi_rcu.h"

volatile int running_cores;
char recv_buf[MAX_MSG_SIZE];

static inline void *
map_memory(const char *test_file, long file_size, void *map_addr)
{
	void *mem;
#ifdef ENABLE_LOCAL_MEMORY
	(void)test_file;
	mem = mmap(map_addr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	printf("bi init: map local memory %p\n", mem);
#else
	int fd;
	fd = open(test_file, O_CREAT | O_RDWR, 0666);
	ftruncate(fd, file_size);
	mem = mmap(map_addr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	printf("bi init: map global memory %p\n", mem);
#endif

#ifdef ENABLE_NON_CC_OP
	printf("bi init: using Non CC memory\n");
#else
	printf("bi init: using CC memory\n");
#endif
	if (mem == MAP_FAILED) {
		printf("mmap failed: file %s sz %ld\n", test_file, file_size);
		exit(-1);
	}
	return mem;
}

static inline void *
__global_init_share(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr)
{
	setup_node_id(node_id);
	setup_node_num(node_num);
	setup_core_num(core_num);
	rpc_init_global();
	return map_memory(test_file, file_size, map_addr);
}

void *
bi_global_init_master(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr, char *test_string)
{
	void *mem, *end;

	mem = __global_init_share(node_id, node_num, core_num, test_file, file_size, map_addr);
	// memset(mem, 0, file_size);
	global_layout = (struct Mem_layout *)mem;
	end = init_global_memory(mem, test_string);
	assert(end - mem < file_size);
	bi_global_rtdsc();
	clwb_range(mem, file_size);
	mem_mgr_init();

	return mem;
}

void *
bi_global_init_slave(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr)
{
	void *mem;

	mem = __global_init_share(node_id, node_num, core_num, test_file, file_size, map_addr);
	clflush_range(mem, file_size);
	global_layout = (struct Mem_layout *)mem;
	return mem;
}

void
bi_local_init_reader(int core_id)
{
	setup_core_id(core_id);
}

void
bi_local_init_server(int core_id, int ncore)
{
	setup_core_id(core_id);
	running_cores = ncore - 1;
}

void
bi_server_run(bi_update_fn_t update_fn, bi_flush_fn_t flush_fn)
{
	uint64_t flush_prev, tsc_prev, curr;
	size_t s;
	int nd, cd;

	flush_prev = bi_local_rdtsc();
	tsc_prev   = bi_local_rdtsc();
	while (running_cores) {
		curr = bi_local_rdtsc();
		if (curr - flush_prev > QUISE_FLUSH_PERIOD) {
			if (flush_fn) flush_fn();
			bi_smr_flush();
			bi_smr_reclaim();
			flush_prev = curr;
		}
		if (curr - tsc_prev > GLOBAL_TSC_PERIOD) {
			bi_global_rtdsc();
			tsc_prev = curr;
		}
		s = rpc_recv_server(recv_buf, &nd, &cd);
		if (!s) continue;
		if (update_fn) update_fn(recv_buf, s, nd, cd);
	}
}
