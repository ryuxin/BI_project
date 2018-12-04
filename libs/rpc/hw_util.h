#ifndef HW_UTIL_H
#define HW_UTIL_H

#include <constant.h>
#include <stdint.h>

/**** TODO ***/
/* gobal rdtsc, core id, node id */

#define BI_ACCESS_ONCE(x)	(*(__volatile__  __typeof__(x) *)&(x))

#define bi_rmb()     __asm__ __volatile__ ("lfence":::"memory")
#define bi_wmb()     __asm__ __volatile__ ("sfence"::: "memory")
#define bi_mb()      __asm__ __volatile__ ("mfence":::"memory")
#define bi_ccb()     __asm__ __volatile__ ("" ::: "memory")

/* x86 cpuid instruction barrier. */
static inline void
bi_inst_bar(void)
{
	int eax, edx, code = 0;
	
	asm volatile("cpuid"
		     :"=a"(eax),"=d"(edx)
		     :"a"(code)
		     :"ecx","ebx");

	return;
}

static inline uint64_t
bi_local_rdtsc(void)
{
	unsigned long a, d;

	asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "ebx", "ecx");

	return ((uint64_t)d << 32) | (uint64_t)a;
}

#include <cc-cache_util.h>
#include <noncc-cache_util.h>

#endif /* HW_UTIL_H */
