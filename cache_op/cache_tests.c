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

#define CACHE_LINE 64
#define PAGE_SIZE  4096
#define MEM_SZ     (1<<26) 	/* 64MB */
#define ITER       256
#define MEM_ITEMS  (MEM_SZ/CACHE_LINE)
#define SIZE	   (MEM_ITEMS * sizeof (struct cache_line) + 2*PAGE_SIZE + MEM_ITEMS*sizeof(int))
#define LOCAL_MEM_TEST

struct cache_line {
	unsigned int v;
	struct cache_line *next; /* random access */
} __attribute__((aligned(CACHE_LINE)));

#ifdef LOCAL_MEM_TEST
struct cache_line mem[MEM_ITEMS];
int rand_mem[MEM_ITEMS];
#else
struct cache_line *mem;
int *rand_mem;
#endif

typedef enum {
	SIZES,
	READ,
	FR, 			/* flush, read */
	FRF, 			/* flush, read, flush */
	WRITE,
	FLUSH,
	FLUSHOPT,
} access_t;

typedef enum {
	SEQUENTIAL,
	RANDOM
} pattern_t;

inline void
clflush(volatile void *p)
{ asm volatile ("clflush (%0)" :: "r"(p) : "memory"); }

inline void
update(volatile void *p)
{ asm volatile ("clflush (%0) ; lfence" :: "r"(p) : "memory"); }

inline void
clflushopt(volatile void *p)
{ asm volatile ("clflushopt (%0)" :: "r"(p) : "memory"); }

inline uint64_t
rdtsc(void)
{
	unsigned long a, d;

	asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "ebx", "ecx");

	return ((uint64_t)d << 32) | (uint64_t)a;
}

void
init_random_access(void)
{
	int i, t, j;

	for (i = 0 ; i < MEM_ITEMS ; i++) {
		rand_mem[i] = (i+1) % MEM_ITEMS;
	}
	for (i = 0 ; i < MEM_ITEMS-1 ; i++) {
		j             = rand() % (MEM_ITEMS-i);
		t             = rand_mem[i];
		rand_mem[i]   = rand_mem[i+j];
		rand_mem[i+j] = t;
	}
	for (i = 0 ; i < MEM_ITEMS ; i++) {
		mem[i].next = &mem[rand_mem[i]];
	}
}

/* in global scope to force the compiler to emit the writes */
unsigned int accum = 0;

static inline void
walk(access_t how, pattern_t pat, size_t sz)
{
	unsigned int i;
	struct cache_line *line = &mem[0];

	assert(sz >= CACHE_LINE);
	for (i = 0 ; i < sz/CACHE_LINE ; i++) {

		switch(pat) {
		case SEQUENTIAL: {
			line = &mem[i];
			break;
		}
		case RANDOM: {
			struct cache_line *next = line->next;

			if (how == FRF) clflushopt(line);
			line = next;
			break;
		}
		}

		switch(how) {
		case READ:     accum    = line->v; break;
		case FR:
		case FRF:      update(line); accum = line->v; break;
		case WRITE:    line->v += i;       break;
		case FLUSH:    clflush(line);      break;
		case FLUSHOPT: clflushopt(line);   break;
		case SIZES:    return;
		}
	}
	if (how == FLUSHOPT || how == FLUSH) asm volatile ("mfence"); /* serialize */
}

/*
 * Perform a number of operations, the last one timed, for a number of different memory sizes.
 */
static inline void
exec(char *name, access_t *how_ops, size_t nops, pattern_t pat)
{
	size_t i;
	int iter;
	uint64_t start, end, tot = 0, overhead;
	static unsigned long sizes[] = {1<<6, 1<<8, 1<<10, 1<<12, 1<<14, 1<<16, 1<<18, 1<<20, 1<<22, 1<<24, 1<<26};
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
				walk(how_ops[j], pat, sizes[i]);
			}
			start = rdtsc();
			walk(how_ops[nops-1], pat, sizes[i]);
			end   = rdtsc();
			tot  += end-start;
		}
		if (how_ops[nops-1] != SIZES) printf("%8lu\t", (tot-overhead)/((sizes[i]/CACHE_LINE)*ITER));
		else                          printf("%8lu\t", sizes[i]);
	}
	printf("\n");
}

int
main(void)
{
//	set_prio();
#ifndef LOCAL_MEM_TEST
	char *file = "/lfs/cache_test";
	int fd = open(file, O_CREAT | O_RDWR, 0666);
	ftruncate(fd, SIZE);
	mem = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	rand_mem = (int *)((char *)mem + MEM_ITEMS * sizeof (struct cache_line) + PAGE_SIZE);
	memset(mem, 0, MEM_ITEMS * sizeof (struct cache_line));
	memset(rand_mem, 0, MEM_ITEMS * sizeof(int));
#endif
//	printf("Cycles per cache-line of the operations last in the list of operations (sequential)\n\n");
	init_random_access();

	exec("Sizes", (access_t[1]){SIZES}, 1, SEQUENTIAL);

//	exec("Warmup", (access_t[3]){READ, READ, READ}, 3, SEQUENTIAL);
//	exec("Read", (access_t[2]){READ, READ}, 2, SEQUENTIAL);
	exec("Read", (access_t[2]){READ, READ}, 2, RANDOM);
//	exec("Flush/Read/Flush", (access_t[2]){READ, FRF}, 2, RANDOM);
	exec("Flush/Read", (access_t[2]){READ, FR}, 2, RANDOM);
	//exec("Modify", (access_t[2]){READ, WRITE}, 2, SEQUENTIAL);

//	exec("Flush + read", (access_t[2]){FLUSHOPT, READ}, 2, SEQUENTIAL);
//	exec("Flush + modify", (access_t[2]){FLUSHOPT, WRITE}, 2, SEQUENTIAL);

	exec("Read + flush", (access_t[2]){READ, FLUSH}, 2, SEQUENTIAL);
	exec("Modify + flush", (access_t[2]){WRITE, FLUSH}, 2, SEQUENTIAL);
	exec("Flush + flush", (access_t[2]){FLUSH, FLUSH}, 2, SEQUENTIAL);

	exec("Read + flushopt", (access_t[2]){READ, FLUSHOPT}, 2, SEQUENTIAL);
	exec("Modify + flushopt", (access_t[2]){WRITE, FLUSHOPT}, 2, SEQUENTIAL);
	exec("Flush + flushopt", (access_t[2]){FLUSH, FLUSHOPT}, 2, SEQUENTIAL);

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
