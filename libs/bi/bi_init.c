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

#define MALLOC_FILE_NAME "/lfs/malloc_data"
#define MALLOC_FILE_SIZE (641*1024*1024*1024UL)
//#define MALLOC_FILE_ADDR ((void *)0x7ff6e8000000)
#define MALLOC_FILE_ADDR ((void *)NULL)

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
	int fd, r;
	fd = open(test_file, O_CREAT | O_RDWR, 0666);
	r  = ftruncate(fd, file_size);
	if (r) {
		printf("ftruncate fail %s\n", test_file);
		exit(-1);
	}
	mem = mmap(map_addr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	printf("bi init: map global memory %p\n", mem);
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
	malloc_area = map_memory(MALLOC_FILE_NAME, MALLOC_FILE_SIZE, MALLOC_FILE_ADDR);
#ifdef ENABLE_NON_CC_OP
	printf("bi init: using Non CC memory\n");
#else
	printf("bi init: using CC memory\n");
#endif
	return map_memory(test_file, file_size, map_addr);
}

void *
bi_global_init_master(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr, char *test_string)
{
	void *mem, *end, *data_area;

	mem = __global_init_share(node_id, node_num, core_num, test_file, file_size, map_addr);
	global_layout = (struct Mem_layout *)mem;
	bi_set_barrier(0);
	memset(mem, 0, file_size);
	end = init_global_memory(mem, test_string, &data_area);
	assert(data_area - mem < file_size);
	//assert(end - mem < file_size);
	bi_global_rtdsc();
	clwb_range(mem, file_size);
	mem_mgr_init();
	bi_set_barrier(1);
	bi_qsc_cache_alloc();

	return mem;
}

void *
bi_global_init_slave(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr)
{
	void *mem;

	mem = __global_init_share(node_id, node_num, core_num, test_file, file_size, map_addr);
	global_layout = (struct Mem_layout *)mem;
	bi_wait_barrier(1);
	clflush_range(mem, file_size);
	bi_qsc_cache_alloc();
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
	int i;

	setup_core_id(core_id);
	running_cores = ncore - 1;
	for(i=0; i<NUM_CORE_PER_NODE; i++) parsec_struct_init(&parsec_time_cache[i]);
}

void
bi_server_run(bi_update_fn_t update_fn, bi_flush_fn_t flush_fn)
{
	uint64_t flush_prev, tsc_prev, curr;
	size_t s;
	int nd, cd, ntot, r;

	ntot = get_active_node_num();
	if (NODE_ID() == 0) {
		for(nd=1; nd<ntot; nd++) {
		        do {
        		        clflush_range(&global_layout->bars[nd], CACHE_LINE);
                		r =  global_layout->bars[nd].barrier;
		        } while(!r);
		}
		global_layout->bars[0].barrier = 1;
	        clwb_range(&global_layout->bars[0], CACHE_LINE);
	} else {
		global_layout->bars[NODE_ID()].barrier = 1;
	        clwb_range(&global_layout->bars[NODE_ID()], CACHE_LINE);
	        do {
        	        clflush_range(&global_layout->bars[0], CACHE_LINE);
                	r =  global_layout->bars[0].barrier;
	        } while(!r);
	}

	flush_prev = bi_local_rdtsc();
	tsc_prev   = bi_local_rdtsc();
	while (running_cores) {
		curr = bi_local_rdtsc();
		if (curr - flush_prev > QUISE_FLUSH_PERIOD) {
			if (flush_fn) flush_fn();
			bi_time_flush();
			bi_smr_flush();
			bi_smr_reclaim();
			flush_prev = curr;
		}
		if (curr - tsc_prev > GLOBAL_TSC_PERIOD) {
			if (NODE_ID() == 0) bi_global_rtdsc();
	                else clflush_range(&global_layout->time, CACHE_LINE);
			tsc_prev = curr;
		}
		s = rpc_recv_server(recv_buf, &nd, &cd);
		if (!s) continue;
		if (update_fn) update_fn(recv_buf, s, nd, cd);
	}
}

void
bi_server_stop(void)
{ running_cores = 0; }

