#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "hw_util.h"

#define MEM_SZ         (1<<26) 	/* 64MB */
#define NUM_SECOND     10
#define MAGIC_TIMER    -1
//#define LOCAL_MEM_TEST

#ifdef LOCAL_MEM_TEST
char mem[MEM_SZ] __attribute__((aligned(CACHE_LINE)));
#else
void *mem;
#endif
char tmem[MEM_SZ];

volatile unsigned long long done = 0, nread = 0, ntick = 0;
volatile int ncache, ntimer;
static volatile uint64_t use_result_dummy = 0;

void
use_int(int result) { use_result_dummy += result; }

void timer_handler(int a)
{
	(void)a;

	ntick++;
	if (ntimer != MAGIC_TIMER) {
		clflush_range(mem, ncache * CACHE_LINE);
	}
	if (ntimer == MAGIC_TIMER || ntick == (unsigned long long)NUM_SECOND*ntimer) {
		printf("#timer %d #cache %d thput %llu\n", ntimer, ncache, nread/NUM_SECOND*64/1024/1024);
		done = 1;
	}
}

int signal_init(void)
{
	struct sigaction act;
	int r;

	memset (&act, 0, sizeof(act));
	act.sa_handler = &timer_handler;
	r = sigaction(SIGALRM, &act, NULL);
	return r;
}

int timer_start(void)
{
	timer_t timer_id;
	int ret;
	struct itimerspec itval;

	ret = timer_create(CLOCK_REALTIME, NULL, &timer_id);
	if (ret) {
		printf("timer creat fails\n");
		return ret;
	}
	if (ntimer == MAGIC_TIMER) {
		itval.it_interval.tv_sec  = NUM_SECOND;
		itval.it_interval.tv_nsec = 0;
		itval.it_value.tv_sec     = NUM_SECOND;
		itval.it_value.tv_nsec    = 0;
	} else {
		itval.it_interval.tv_sec  = 0;
		itval.it_interval.tv_nsec = 1000000000/ntimer;
		itval.it_value.tv_sec     = 0;
		itval.it_value.tv_nsec    = 1000000000/ntimer;
	}
	ret = timer_settime(timer_id, 0, &itval, NULL);
	return ret;
}

void bench(int flush)
{
        register int *lastone = (int *)((char *)mem + ncache*CACHE_LINE);
        register int sum = 0;

	while (!done) {
		if (flush) clflush_range(mem, ncache*CACHE_LINE);

                register int *p = mem;
                while (p <= lastone) {
                        sum +=
#define DOIT(i) p[i]+
                        DOIT(0) DOIT(4) DOIT(8) DOIT(12) DOIT(16) DOIT(20) DOIT(24)
                        DOIT(28) DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52)
                        DOIT(56) DOIT(60) DOIT(64) DOIT(68) DOIT(72) DOIT(76)
                        DOIT(80) DOIT(84) DOIT(88) DOIT(92) DOIT(96) DOIT(100)
                        DOIT(104) DOIT(108) DOIT(112) DOIT(116) DOIT(120)
                        p[124];
                        p +=  128;
                }
                use_int(sum);
//		memcpy(tmem, mem, ncache*CACHE_LINE);
		nread += ncache;
	}
}

int main(int argc, char *argv[])
{
	int r;
	int flush;
#ifndef LOCAL_MEM_TEST
	char *file = "/lfs/cache_test";
	int fd = open(file, O_CREAT | O_RDWR, 0666);
	ftruncate(fd, MEM_SZ);
	mem = mmap(0, MEM_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
#endif

	if (argc != 4) {
		printf("usage: %s #timer #cache flush\n", argv[0]);
		exit(-1);
	}
	ntimer = atoi(argv[1]);
	ncache = atoi(argv[2]);
	flush  = atoi(argv[3]);
	if (ncache > MEM_SZ/CACHE_LINE) {
		printf("working set is larger than max file size\n");
		exit(-1);
	}

	r = signal_init();
	if (r) {
		printf("signal init error\n");
		goto ret;
	}

	r = timer_start();
	if (r) goto ret;
	bench(flush);
ret:
#ifndef LOCAL_MEM_TEST
	munmap(mem, MEM_SZ);
	close(fd);
#endif
	return 0;
}

