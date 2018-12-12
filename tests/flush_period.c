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
#define NUM_SECOND     60
#define MAGIC_TIMER    -1
#define ENABLE_ALWAYS_FLUSH
#define LOCAL_MEM_TEST

#ifdef LOCAL_MEM_TEST
char mem[MEM_SZ] __attribute__((aligned(CACHE_LINE)));
#else
char *mem;
#endif
char tmem[2*CACHE_LINE];

volatile unsigned long long done = 0, nread = 0, ntick = 0;
volatile int ncache, ntimer;

void timer_handler(int a)
{
	(void)a;

	ntick++;
	if (ntimer != MAGIC_TIMER) {
		clflush_range(mem, ncache * CACHE_LINE);
	}
	if (ntimer == MAGIC_TIMER || ntick == (unsigned long long)NUM_SECOND*ntimer) {
		printf("#timer %d #cache %d thput %llu\n", ntimer, ncache, nread/NUM_SECOND);
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
	ret = timer_settime (timer_id, 0, &itval, NULL);
	return ret;
}

void bench(void)
{
	int i;
	char *pos;

	while (!done) {
		pos = mem;
		for (i=0; i<ncache; i++) {
#ifdef ENABLE_ALWAYS_FLUSH
			clflush_range(pos, CACHE_LINE);
#endif
			memcpy(tmem, pos, CACHE_LINE);
			pos  += CACHE_LINE;
			nread++;
		}
	}
}

int main(int argc, char *argv[])
{
	int r;
#ifndef LOCAL_MEM_TEST
	char *file = "/lfs/cache_test";
	int fd = open(file, O_CREAT | O_RDWR, 0666);
	ftruncate(fd, MEM_SZ);
	mem = mmap(0, MEM_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	memset(mem, 0, MEM_SZ);
#endif

	if (argc != 3) {
		printf("usage: %s #timer #cache\n", argv[0]);
		exit(-1);
	}
	ntimer   = atoi(argv[1]);
	ncache   = atoi(argv[2]);
	if (ncache > MEM_SZ/CACHE_LINE) {
		printf("working set is larger than max file size\n");
		exit(-1);
	}
#ifdef ENABLE_ALWAYS_FLUSH
	assert(ntimer == MAGIC_TIMER);
#endif

	r = signal_init();
	if (r) {
		printf("signal init error\n");
		goto ret;
	}

	r = timer_start();
	if (r) goto ret;
	bench();
ret:
#ifndef LOCAL_MEM_TEST
	munmap(mem, MEM_SZ);
	close(fd);
#endif
	return 0;
}

