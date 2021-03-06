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

#define bi_likely(x)      __builtin_expect(!!(x), 1)
#define bi_unlikely(x)    __builtin_expect(!!(x), 0)
#define round_to_pow2(x, pow2)    (((unsigned long)(x))&(~((pow2)-1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)x)+(pow2)-1, (pow2)))
#define round_to_cacheline(x)     round_to_pow2(x, CACHE_LINE)
#define round_to_page(x)          round_to_pow2(x, PAGE_SIZE)
#define round_up_to_page(x)       round_up_to_pow2(x, PAGE_SIZE)

#define NODE_ID() (local_node_id)
#define CORE_ID() (local_core_id)

#define BI_ACCESS_ONCE(x)	(*(__volatile__  __typeof__(x) *)&(x))

#define bi_rmb()     __asm__ __volatile__ ("lfence":::"memory")
#define bi_wmb()     __asm__ __volatile__ ("sfence"::: "memory")
#define bi_mb()      __asm__ __volatile__ ("mfence":::"memory")
#define bi_ccb()     __asm__ __volatile__ ("" ::: "memory")

void load_trace(long nops, unsigned int percent_update, char *ops);
uint64_t bi_global_rtdsc();
int convert_to_core_id(int nid, int cid);
void thd_set_affinity(pthread_t tid, int nid, int cid);
void thd_set_affinity_to_core(pthread_t tid, int core);
extern int dbg_r;
void start_time(void);
void end_time(int n);

void dbg_log_init(void);
void dbg_log_add(char *s, void *p);
void dbg_log_flush(void);

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
