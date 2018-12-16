#ifndef NON_CC_CACHE_UTIL_H
#define NON_CC_CACHE_UTIL_H
#ifdef ENABLE_NON_CC_OP

#include "constant.h"

static inline void
bi_flush_cache(void *p)
{
#ifdef ENABLE_CLFLUSHOPT
	__asm__ __volatile__("clflushopt (%0)" :: "r"(p));
#else
	__asm__ __volatile__("clflush (%0)" :: "r"(p));
#endif
}

static inline void
bi_wb_cache(void *p)
{
	__asm__ __volatile__("clwb (%0)" :: "r"(p));
}

static inline void
clflush_range_opt(void *s, size_t sz)
{
	void *e;
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(s + sz);
	for(; s<=e; s += CACHE_LINE) bi_flush_cache(s);
}

static inline void
clflush_range(void *s, size_t sz)
{
	clflush_range_opt(s, sz);
	bi_wmb(); /* serialize */
}

static inline void
clwb_range_opt(void *s, size_t sz)
{
	void *e;
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(s + sz);
	for(; s<=e; s += CACHE_LINE) bi_wb_cache(s);
}

static inline void
clwb_range(void *s, size_t sz)
{
	clwb_range_opt(s, sz);
	bi_wmb(); /* serialize */
}

/************** FIXME: adapt to gru version *****/
/*
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
/*
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
*/

#endif
#endif /* NON_CC_CACHE_UTIL_H */
