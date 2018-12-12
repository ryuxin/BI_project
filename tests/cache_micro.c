#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define _GNU_SOURCE
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "hw_util.h"

static void
call_getrlimit(int id, char *name)
{
	struct rlimit rl;
	(void)name;

	if (getrlimit(id, &rl)) {
		perror("getrlimit: ");
		exit(-1);
	}
}

static void
call_setrlimit(int id, rlim_t c, rlim_t m)
{
	struct rlimit rl;

	rl.rlim_cur = c;
	rl.rlim_max = m;
	if (setrlimit(id, &rl)) {
		exit(-1);
	}
}

void
set_prio(void)
{
	struct sched_param sp;

	call_getrlimit(RLIMIT_CPU, "CPU");
#ifdef RLIMIT_RTTIME
	call_getrlimit(RLIMIT_RTTIME, "RTTIME");
#endif
	call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
	call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	call_getrlimit(RLIMIT_NICE, "NICE");

	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
		exit(-1);
	}
	sp.sched_priority = sched_get_priority_max(SCHED_RR);
	if (sched_setscheduler(0, SCHED_RR, &sp) < 0) {
		perror("setscheduler: ");
		exit(-1);
	}
	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
		exit(-1);
	}
	assert(sp.sched_priority == sched_get_priority_max(SCHED_RR));

	return;
}

#define MEM_SZ     (1<<26) 	/* 64MB */
#define ITER       256
#define LOCAL_MEM_TEST

#ifdef LOCAL_MEM_TEST
char mem[MEM_SZ];
#else
char *mem;
#endif
char tmem[MEM_SZ];

typedef enum {
	SIZES,
	READ,
	WRITE,
	FLUSH,
	CLWB,
	FLUSHOPT,
} access_t;


static inline void
clflush_cacheline(void *p)
{
	__asm__ __volatile__("clflush (%0)" :: "r"(p));
}

static inline void
clflush_range_slow(void *s, size_t sz)
{
	void *e;
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(s + sz);
	for(; s<=e; s += CACHE_LINE) clflush_cacheline(s);
	bi_wmb(); /* serialize */
}

inline uint64_t
rdtsc(void)
{
	unsigned long a, d;

	asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "ebx", "ecx");

	return ((uint64_t)d << 32) | (uint64_t)a;
}

static inline void
walk(access_t how, size_t sz)
{
	unsigned int i;

	assert(sz >= CACHE_LINE);

	switch(how) {
	case READ:        memcpy(tmem, mem, sz);            break;
	case WRITE:       memcpy(mem, tmem, sz);            break;
	case FLUSH:       clflush_range_slow(mem, sz);      break;
	case CLWB:        clwb_range(mem, sz);              break;
	case FLUSHOPT:    clflush_range(mem, sz);           break;
	case SIZES:    return;
	}
}

/*
 * Perform a number of operations, the last one timed, for a number of different memory sizes.
 */
static inline void
exec(char *name, access_t *how_ops, size_t nops)
{
	size_t i;
	static unsigned long sizes[] = {1<<6, 1<<8, 1<<10, 1<<12, 1<<14, 1<<16, 1<<18, 1<<20, 1<<22, 1<<24, 1<<26};
	int iter;
	uint64_t start, end, tot = 0, overhead;
	(void)name;

	for (iter = 0 ; iter < ITER ; iter++) {
		start = rdtsc();
		end   = rdtsc();
		assert(end-start > 0);
		tot  += end-start;
	}
	overhead = tot;
	tot      = 0;

	assert(how_ops && nops > 0);
#ifndef NO_PRINT_TITLES
	printf("%20s\t", name);
#endif
	for (i = 0 ; i < sizeof(sizes)/sizeof(unsigned long) ; i++) {
		for (iter = 0 ; iter < ITER ; iter++) {
			unsigned int j;

			for (j = 0 ; j < nops-1 ; j++) {
				walk(how_ops[j], sizes[i]);
			}
			start = rdtsc();
			walk(how_ops[nops-1], sizes[i]);
			end   = rdtsc();
			tot  += end-start;
		}
		if (how_ops[nops-1] != SIZES) printf("%11lu ", (tot-overhead)/ITER;
		else                          printf("%11lu ", sizes[i]);
	}
	printf("\n");
}

int
main(void)
{
//	set_prio();
	size_t i;
#ifndef LOCAL_MEM_TEST
	char *file = "/lfs/cache_test";
	int fd = open(file, O_CREAT | O_RDWR, 0666);
	ftruncate(fd, MEM_SZ);
	mem = mmap(0, MEM_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	memset(mem, 0, MEM_SZ);
#endif
//	printf("Cycles per cache-line of the operations last in the list of operations (sequential)\n\n");

	exec("Sizes", (access_t[1]){SIZES}, 1);
	exec("Read + Read", (access_t[2]){READ, READ}, 2);
	exec("Modify + Read", (access_t[2]){WRITE, READ}, 2);
	exec("Flush + Read", (access_t[2]){FLUSHOPT, READ}, 2);

	exec("Read + Modify", (access_t[2]){READ, WRITE}, 2);
	exec("Modify + Modify", (access_t[2]){WRITE, WRITE}, 2);
	exec("Flush + Modify", (access_t[2]){FLUSHOPT, WRITE}, 2);

	exec("Read + flush", (access_t[2]){READ, FLUSH}, 2);
	exec("Modify + flush", (access_t[2]){WRITE, FLUSH}, 2);
	exec("Flush + flush", (access_t[2]){FLUSHOPT, FLUSH}, 2);

	exec("Read + flushopt", (access_t[2]){READ, FLUSHOPT}, 2);
	exec("Modify + flushopt", (access_t[2]){WRITE, FLUSHOPT}, 2);
	exec("Flush + flushopt", (access_t[2]){FLUSHOPT, FLUSHOPT}, 2);

	exec("Read + clwb", (access_t[2]){READ, CLWB}, 2);
	exec("Modify + clwb", (access_t[2]){WRITE, CLWB}, 2);
	exec("Flush + clwb", (access_t[2]){FLUSHOPT, CLWB}, 2);
	/* printf("Cycles per page of the operations last in the list of operations (random)\n\n"); */

	/* exec("Sizes", (access_t[1]){SIZES}, 1, RANDOM); */

	/* exec("Fault in memory", (access_t[1]){READ}, 1, RANDOM); */

	/* exec("Read", (access_t[1]){READ}, 1, RANDOM); */
	/* exec("Flush + read", (access_t[2]){FLUSHOPT, READ}, 2, RANDOM); */

	/* exec("Modify", (access_t[1]){WRITE}, 1, RANDOM); */
	/* exec("Flush + modify", (access_t[2]){FLUSHOPT, WRITE}, 2, RANDOM); */

	/* exec("Read + flush", (access_t[2]){READ, FLUSH}, 2, RANDOM); */
	/* exec("Modify + flush", (access_t[2]){WRITE, FLUSH}, 2, RANDOM); */
	/* exec("Flush + flush", (access_t[2]){FLUSH, FLUSH}, 2, RANDOM); */

	/* exec("Read + flushopt", (access_t[2]){READ, FLUSHOPT}, 2, RANDOM); */
	/* exec("Modify + flushopt", (access_t[2]){WRITE, FLUSHOPT}, 2, RANDOM); */
	/* exec("Flush + flushopt", (access_t[2]){FLUSH, FLUSHOPT}, 2, RANDOM); */

	return 0;
}
