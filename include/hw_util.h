#ifndef HW_UTIL_H
#define HW_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include "constant.h"

extern int local_node_id;
extern int num_node_in_use;
extern int num_core_in_use;
extern __thread int local_core_id;

#define NODE_ID() (local_node_id)
#define CORE_ID() (local_core_id)

#define BI_ACCESS_ONCE(x)	(*(__volatile__  __typeof__(x) *)&(x))

#define bi_rmb()     __asm__ __volatile__ ("lfence":::"memory")
#define bi_wmb()     __asm__ __volatile__ ("sfence"::: "memory")
#define bi_mb()      __asm__ __volatile__ ("mfence":::"memory")
#define bi_ccb()     __asm__ __volatile__ ("" ::: "memory")

void load_trace(long nops, unsigned int percent_update, char *ops);
uint64_t bi_global_rtdsc();
void thd_set_affinity(pthread_t tid, int nid, int cid);
extern int dbg_r;
void start_time(void);
void end_time(int n);

/* x86 cpuid instruction barrier. */
static inline void
bi_inst_bar(void)
{
	int eax, edx, code = 0;
	
	__asm__ __volatile__ ("cpuid"
		     :"=a"(eax),"=d"(edx)
		     :"a"(code)
		     :"ecx","ebx");

	return;
}

static inline uint64_t
bi_local_rdtsc(void)
{
	unsigned long a, d;
	__asm__ __volatile__ ("rdtscp" : "=a" (a), "=d" (d) : : "ebx", "ecx");
	return ((uint64_t)d << 32) | (uint64_t)a;
}

static inline void
setup_node_id(int nid)
{
	local_node_id = nid;
}

static inline void
setup_node_num(int num)
{
	assert(num <= NUM_NODES);
	num_node_in_use = num;
}

static inline void
setup_core_id(int cid)
{
	local_core_id = cid;
}

static inline void
setup_core_num(int num)
{
	assert(NUM_CORE_PER_NODE);
	num_core_in_use = num;
}
/********* TODO remove assert in final version *********/
static inline int
get_active_node_num(void)
{
	assert(num_node_in_use);
	return num_node_in_use;
}

static inline int
get_active_core_num(void)
{
	assert(num_core_in_use);
	return num_core_in_use;
}

#include "cc-cache_util.h"
#include "noncc-cache_util.h"

#endif /* HW_UTIL_H */
