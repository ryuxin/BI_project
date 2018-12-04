#ifndef BI_POINTER_H
#define BI_POINTER_H

/*
 * read side interface.
 * copy memory with sz from src (global memory) to dst (local memory).
 * This invalidates src before copy, so get up-to-date value.
 * There is NO read log to record src.
 */
static inline void
bi_reference_area_aggressive(void *dst, void *src, int sz);

/*
 * read side interface.
 * copy memory with sz from src (global memory) to dst (local memory).
 * This DOES NOT invalidates src before copy, so get possible stale value.
 * There is NO read log to record src.
 */
static inline void
bi_reference_area_lazy(void *dst, void *src, int sz);

/*
 * write side interface.
 * copy memory with sz from src (local memory) to dst (global memory).
 * This writes back dst to memroy after copy.
 * There is NO write log to record dst.
 */
static inline void
bi_publish_area(void *dst, void *src, int sz);


/*********** TODO **************/
/* bi pointer api */

#endif /* BI_POINTER_H */
