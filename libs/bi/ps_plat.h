#ifndef PS_PLAT_H
#define PS_PLAT_H


#define u16_t unsigned short int
#define u32_t unsigned int
#define u64_t unsigned long long

typedef u64_t ps_tsc_t; 	/* our time-stamp counter representation */
typedef u16_t coreid_t;
typedef u16_t localityid_t;

#define PS_CACHE_PAD   (PS_CACHE_LINE*2)
#define PS_WORD        sizeof(long)
#define PS_PACKED      __attribute__((packed))
#define PS_ALIGNED     __attribute__((aligned(PS_CACHE_LINE)))
#define PS_WORDALIGNED __attribute__((aligned(PS_WORD)))
#ifndef PS_NUMCORES
#define PS_NUMCORES      10
#endif
#ifndef PS_NUMLOCALITIES
#define PS_NUMLOCALITIES 2
#endif
#define PS_RNDUP(v, a) (-(-(v) & -(a))) /* from blogs.oracle.com/jwadams/entry/macros_and_powers_of_two */

/*
 * How frequently do we check remote free lists when we make an
 * allocation?  This is in platform-specific code because it is
 * dependent on the hardware costs for cache-line contention on a
 * remote numa node.
 *
 * If that contention has 16x the cost of a normal allocation, for
 * example, then choosing to batch checking remote frees once every
 * 128 iterations increases allocation cost by a factor of (2^4/2^7 =
 * 2^-3) 1/8.
 */
#ifndef PS_REMOTE_BATCH
/* Needs to be a power of 2 */
#define PS_REMOTE_BATCH 64
#endif

/* Default allocation and deallocation functions */
static inline void *
ps_plat_alloc(size_t sz, coreid_t coreid)
{
	return NULL;
}

static inline void
ps_plat_free(void *s, size_t sz, coreid_t coreid)
{
	return ;
}

static inline ps_tsc_t
ps_tsc_locality(coreid_t *coreid, localityid_t *numaid)
{
	return 0;
}

static inline unsigned int
ps_coreid(void)
{
	return 0;
}





#endif	/* PS_PLAT_H */
