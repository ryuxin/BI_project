#ifndef HW_UTIL_H
#define HW_UTIL_H

#include <constant.h>
#include <stdint.h>
#include <cc-cache_util.h>
#include <noncc-cache_util.h>

/**** TODO split to cc and non-cc/gru version for atomic ***/
/* memoory barrier, compiler barrier, gobal rdtsc, core id, node id */
/* https://github.com/urcu/userspace-rcu/blob/master/include/urcu/arch/x86.h */

static inline void 
bi_compiler_barrier(void)
{
	asm volatile("" ::: "memory");
}

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

#endif /* HW_UTIL_H */
