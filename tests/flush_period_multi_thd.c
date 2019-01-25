#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define _GNU_SOURCE
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "hw_util.h"

#define MEM_SZ  (1<<26) 	/* 64MB */
#define NUM_SEC 10

void *mem;
char tmem[MEM_SZ];
volatile int done;
volatile uint64_t period;
volatile size_t flush_sz;
static volatile uint64_t use_result_dummy = 0;

void
use_int(int result) { use_result_dummy += result; }

void *
flush_thd_fn(void *arg)
{
	uint64_t flush_prev, curr;
	(void)arg;

	thd_set_affinity(pthread_self(), 0, 2);
	flush_prev = bi_local_rdtsc();
	while (!done) {
		curr = bi_local_rdtsc();
		if (curr - flush_prev > period) {
//printf("dbg lfush thd\n");
			clflush_range(mem, flush_sz);
			flush_prev = curr;
		}
	}
	return NULL;
}

static unsigned long
load_thput(size_t sz, int flush)
{
	uint64_t s, e, curr = 0;
	unsigned long nr = 0, pnr = 0;
	register int *lastone = (int *)((char *)mem + sz);
	register int sum = 0;

	s = bi_local_rdtsc();
	e = s + NUM_SEC*CPU_HZ;
	do {
		if (flush) clflush_range(mem, sz);

		register int *p = mem;
		while (p <= lastone) {
			sum += 
#define	DOIT(i)	p[i]+
			DOIT(0) DOIT(4) DOIT(8) DOIT(12) DOIT(16) DOIT(20) DOIT(24)
			DOIT(28) DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52)
			DOIT(56) DOIT(60) DOIT(64) DOIT(68) DOIT(72) DOIT(76)
			DOIT(80) DOIT(84) DOIT(88) DOIT(92) DOIT(96) DOIT(100)
			DOIT(104) DOIT(108) DOIT(112) DOIT(116) DOIT(120) 
			p[124];
			p +=  128;
		}
		use_int(sum);
		//memcpy(tmem, mem, sz);
		nr += sz;
		if (nr - pnr >= 1024*1024*1024) {
			pnr = nr;
			curr = bi_local_rdtsc();
		}
	} while (curr < e);
//	printf("log time diff %lu\n", curr - e);

	return nr/NUM_SEC/1024/1024;
}

static void
exec(char *title, size_t *szs, int nsz, int flush)
{
	int i, ret;
	unsigned long r;
	pthread_t pthd;

	printf("%s ", title);
	for(i=0; i<nsz; i++) {
		done     = 0;
		flush_sz = szs[i];
		ret      = pthread_create(&pthd, 0, flush_thd_fn, NULL);
		if (ret) {
			printf("create flush thd failed\n");
			exit(-1);
		}
		r = load_thput(szs[i], flush);
		printf("%lu ", r);
		done = 1;
		pthread_join(pthd, (void *)&ret);
	}
	printf("\n");
}
int
main(void)
{
	size_t szs[] = {1<<12, 1<<19, 1<<23, 1<<26}, i, nsz;
	char *file = "/lfs/cache_test";
	int fd = open(file, O_CREAT | O_RDWR, 0666);

	ftruncate(fd, MEM_SZ);
	mem = mmap(0, MEM_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	thd_set_affinity(pthread_self(), 0, 0);
	nsz = sizeof(szs)/sizeof(size_t);
	printf("SIZES ");
	for(i=0; i<nsz; i++) printf("%lu ", szs[i]);
	printf("\n");

	period = 2*NUM_SEC*CPU_HZ;
	exec("  AGG", szs, nsz, 1);

	period = CPU_HZ/100;
	exec(" 10ms", szs, nsz, 0);

	period = CPU_HZ/10;
	exec("100ms", szs, nsz, 0);

	period = 2*NUM_SEC*CPU_HZ;
	exec("   NO", szs, nsz, 0);
	return 0;
}
