#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sched.h>
#include <pthread.h>
#include "mem_mgr.h"

#define TRACE_NAME_LEN 50
#define NUM_SOCKETS 4
#define NUM_CORE_PER_SOCKET 28

int local_node_id;
int num_node_in_use;
int num_core_in_use;
__thread int local_core_id;
const int core_map[NUM_SOCKETS][NUM_CORE_PER_SOCKET] = {
			{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
			{28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55},
			{56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83},
			{84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111}};

int
convert_to_core_id(int nid, int cid)
{
	assert(cid < NUM_CORE_PER_SOCKET);
	return core_map[nid % NUM_SOCKETS][cid];
}

void
thd_set_affinity(pthread_t tid, int nid, int cid)
{
	cpu_set_t s;
	int ret, cpuid;

	cpuid = convert_to_core_id(nid, cid);
	CPU_ZERO(&s);
	CPU_SET(cpuid, &s);

	ret = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &s);
	if (ret) {
		perror("setting affinity error\n");
		exit(-1);
	}
}

static void
trace_gen(int fd, long nops, unsigned int percent_update)
{
	unsigned int i;
	srand(time(NULL));
	for (i = 0 ; i < nops ; i++) {
		char value;
		if ((unsigned int)rand() % 100 < percent_update) value = 'U';
		else                               value = 'R';
		if (write(fd, &value, 1) < 1) {
			perror("Writing to trace file");
			exit(-1);
		}
	}
	lseek(fd, 0, SEEK_SET);
}

void
load_trace(long nops, unsigned int percent_update, char *ops)
{
	int fd, ret;
	int bytes;
	long i, n_read, n_update;
	char trace_name[TRACE_NAME_LEN];

	sprintf(trace_name, "/tmp/%u_update.dat", percent_update);
	ret = mlock(ops, nops);
	if (ret) {
		printf("Cannot lock memory (%d). Check privilege (i.e. use sudo). Exit.\n", ret);
		exit(-1);
	}

	printf("loading trace file @ %s.\n", trace_name);
	/* read the entire trace into memory. */
	fd = open(trace_name, O_RDONLY);
	if (fd < 0) {
		fd = open(trace_name, O_CREAT | O_RDWR, S_IRWXU);
		assert(fd >= 0);
		trace_gen(fd, nops, percent_update);
	}

	bytes = read(fd, &ops[0], nops);
	assert(bytes == nops);
	n_read = n_update = 0;

	for (i = 0 ; i < nops ; i++) {
		if      (ops[i] == 'R') { ops[i] = 0; n_read++; }
		else if (ops[i] == 'U') { ops[i] = 1; n_update++; }
		else assert(0);
	}
	printf("Trace: read %ld, update %ld, total %ld\n", n_read, n_update, (n_read+n_update));
	assert(n_read+n_update == nops);

	close(fd);
	return;
}

uint64_t
bi_global_rtdsc()
{
	if (NODE_ID() == 0) {
		global_layout->time.tsc = bi_local_rdtsc();
		clwb_range(&global_layout->time, CACHE_LINE);
	} else {
		clflush_range(&global_layout->time, CACHE_LINE);
	}
	return global_layout->time.tsc;
}
