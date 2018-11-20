#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sched.h>
#include <stdint.h>
#include <assert.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>

#define ENABLE_CC_OP
#define CAS_SUCCESS 1
#define ENABLE_CLFLUSHOPT 1

static inline void 
cos_compiler_barrier(void)
{
	asm volatile("" ::: "memory");
}

/* 
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
static inline int 
cos_cas(unsigned long *target, unsigned long old, unsigned long updated)
{
	char z;
	__asm__ __volatile__("lock cmpxchgl %2, %0; setz %1"
			     : "+m" (*target),
			       "=a" (z)
			     : "q"  (updated),
			       "a"  (old)
			     : "memory", "cc");
	return (int)z;
}

/* Fetch-and-add implementation on x86. It returns the original value
 * before xaddl. */
static inline int 
cos_faa(int *var, int value)
{
	asm volatile("lock xaddl %%eax, %2;"
		     :"=a" (value)            //Output
		     :"a" (value), "m" (*var) //Input
		     :"memory");
	return value;
}

/* x86 cpuid instruction barrier. */
static inline void
cos_inst_bar(void)
{
	int eax, edx, code = 0;
	
	asm volatile("cpuid"
		     :"=a"(eax),"=d"(edx)
		     :"a"(code)
		     :"ecx","ebx");

	return;
}

static inline void
cos_flush_cache(void *p)
{
#ifdef ENABLE_CC_OP
#ifdef ENABLE_CLFLUSHOPT
	__asm__ __volatile__("clflushopt (%0)" :: "r"(p));
#else
	__asm__ __volatile__("clflush (%0)" :: "r"(p));
#endif
#endif
	return ;
}

static inline void
cos_wb_cache(void *p)
{
#ifdef ENABLE_CC_OP
#ifdef ENABLE_CLFLUSHOPT
	__asm__ __volatile__("clflushopt (%0)" :: "r"(p));
#else
	__asm__ __volatile__("clflush (%0)" :: "r"(p));
#endif
#endif
	return ;
}

static inline void
clflush_range(void *s, void *e)
{
#ifdef ENABLE_CC_OP
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(e-1);
	for(; s<=e; s += CACHE_LINE) cos_flush_cache(s);
	asm volatile ("sfence"); /* serialize */
#endif
	return ;
}

static inline void
clwb_range_opt(void *s, void *e)
{
#ifdef ENABLE_CC_OP
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(e-1);
	for(; s<=e; s += CACHE_LINE) cos_wb_cache(s);
#endif
	return ;
}

static inline void
clwb_range(void *s, void *e)
{
#ifdef ENABLE_CC_OP
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(e-1);
	for(; s<=e; s += CACHE_LINE) cos_wb_cache(s);
	asm volatile ("sfence"); /* serialize */
#endif
	return ;
}

/* load/store a value of other nodes */
static inline int
non_cc_load_int(int *target)
{
#ifdef ENABLE_CC_OP
	clflush_range(target, (char *)target+CACHE_LINE);
#endif
	return *(volatile int *)target;
}

static inline void
non_cc_store_int(int *target, int value)
{
	*(volatile int *)target = value;
#ifdef ENABLE_CC_OP
	clwb_range(target, (char *)target+CACHE_LINE);
#endif
}

static inline int
cc_load_int(int *target)
{
	return *(volatile int *)target;
}

static inline void
cc_store_int(int *target, int value)
{
	*(volatile int *)target = value;
}

static inline int
hv2node(uint32_t hv)
{
	uint32_t tot = hashmask(HASHPOWER_DEFAULT);
	return (hv & tot) > (tot/(NUM_NODE/2));
}

#ifndef rdtscll
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#endif

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

static void
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

#endif
