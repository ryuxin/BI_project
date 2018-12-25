#ifndef BI_POINTER_H
#define BI_POINTER_H

#include <string.h>
#include "hw_util.h"

/*
 * read side interface.
 * copy memory with sz from src (global memory) to dst (local memory).
 * This DOES NOT invalidates src before copy, so get possible stale value.
 * There is NO read log to record src.
 */
static inline void
bi_dereference_area_lazy(void *dst, void *src, size_t sz)
{
	memcpy(dst, src, sz);
	bi_wmb();
}

/*
 * read side interface.
 * copy memory with sz from src (global memory) to dst (local memory).
 * This invalidates src before copy, so get up-to-date value.
 * There is NO read log to record src.
 */
static inline void
bi_dereference_area_aggressive(void *dst, void *src, size_t sz)
{
	clflush_range(src, sz);
	bi_dereference_area_lazy(dst, src, sz);
}

/*
 * write side interface.
 * copy memory with sz from src (local memory) to dst (global memory).
 * This writes back dst to memroy after copy.
 * There is NO write log to record dst.
 */
static inline void
bi_publish_area(void *dst, void *src, size_t sz)
{
	memcpy(dst, src, sz);
	clwb_range(dst, sz);
}


/*********** TODO **************/
/* bi publish pointer maintain quisence queue and write log*/

/*
#define __dereference(p)						\
				__extension__				\
				({					\
				__typeof__(p) __p = BI_ACCESS_ONCE(p); \
				(__p);				\
				})
*/

/*
 * read side interface.
 * dereference ptr which is in global memory.
 * This DOES NOT invalidates ptr before dereference, so get possible stale value.
 * There is NO read log to record ptr and *ptr. 
 */
static inline void *
bi_dereference_pointer_lazy(void *ptr)
{
	__typeof__(ptr) __p = BI_ACCESS_ONCE(ptr);
	return (void *)__p;
}

/*
 * read side interface.
 * dereference ptr which is in global memory.
 * This invalidates ptr before dereference, so get up-to-date value.
 * There is NO read log to record ptr and *ptr.
 */
static inline void *
bi_dereference_pointer_aggressive(void *ptr)
{
	bi_flush_cache(ptr);
	return bi_dereference_pointer_lazy(ptr);
}

/*
 * write side interface.
 * set ptr with v.
 * This writes back ptr to memroy.
 * There is NO write log to record ptr and *ptr.
 */
static inline void
bi_publish_pointer(void **ptr, void *v)
{
	bi_wmb();
	*ptr = v;
	bi_wb_cache(ptr);
	bi_wmb();
}

#endif /* BI_POINTER_H */
