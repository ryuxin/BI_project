#ifndef CC_CACHE_UTIL_H
#define CC_CACHE_UTIL_H
#ifndef ENABLE_NON_CC_OP

#include "constant.h"

static inline void
bi_flush_cache(void *p)
{
	(void)p;
}

static inline void
bi_wb_cache(void *p)
{
	(void)p;
}

static inline void
clflush_range(void *s, size_t sz)
{
	(void)s;
	(void)sz;
}

static inline void
clwb_range_opt(void *s, size_t sz)
{
	(void)s;
	(void)sz;
}

static inline void
clwb_range(void *s, size_t sz)
{
	(void)s;
	(void)sz;
}

#define PS_ATOMIC_POSTFIX "q" /* x86-64 */
#define PS_CAS_INSTRUCTION "cmpxchg"
#define PS_FAA_INSTRUCTION "xadd"
#define PS_CAS_STR PS_CAS_INSTRUCTION PS_ATOMIC_POSTFIX " %2, %0; setz %1"
#define PS_FAA_STR PS_FAA_INSTRUCTION PS_ATOMIC_POSTFIX " %1, %0"

/*
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
static inline int
bi_cas(unsigned long *target, unsigned long old, unsigned long updated)
{
        char z;
        __asm__ __volatile__("lock " PS_CAS_STR
                             : "+m" (*target), "=a" (z)
                             : "q"  (updated), "a"  (old)
                             : "memory", "cc");
        return (int)z;
}

static inline long
bi_faa(unsigned long *target, long inc)
{
        __asm__ __volatile__("lock " PS_FAA_STR
                             : "+m" (*target), "+q" (inc)
                             : : "memory", "cc");
        return inc;
}

#endif
#endif /* CC_CACHE_UTIL_H */
