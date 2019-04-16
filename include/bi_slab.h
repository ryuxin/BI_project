#ifndef  PS_SLAB_H
#define  PS_SLAB_H

#include "ps_list.h"

#define PS_RNDUP(v, a)      (-(-(v) & -(a))) 
/* #define PS_SLAB_DEBUG 1 */

typedef uint64_t ps_tsc_t; 	/* our time-stamp counter representation */
struct ps_slab_info;

/* The header for a slab. */
struct ps_slab {
	void  *memory;		/* != NULL iff slab is separately allocated */
	size_t memsz;	/* size of backing memory */
	struct Free_mem_item *mem_item;
	struct ps_slab_info *si;
	char   pad[CACHE_LINE-3*sizeof(void *)+sizeof(size_t)];

	/* Frequently modified data on the owning core... */
	struct ps_mheader *freelist; /* free objs in this slab */
	struct ps_list     list;     /* freelist of slabs */
	size_t             nfree;    /* # allocations in freelist */
}__attribute__((packed));

struct ps_slab_freelist {
	struct ps_slab    *list;
};

typedef enum {
	SLAB_IN_USE  = 0,
	SLAB_FREE    = 1ULL
} slab_type_t;

/* Memory header */
struct ps_mheader {
	ps_tsc_t type;
	struct ps_slab    *slab;   /* slab header ptr */
	struct ps_mheader *next;   /* slab freelist ptr */
} __attribute__((packed));

struct ps_slab_info {
	struct ps_slab_freelist fl;	 /* freelist of slabs with available objects */
	struct ps_slab_freelist el;	 /* freelist of slabs with no available objects */
	int  nodeid;	             /* which is the home node for these slabs? */
	size_t obj_sz;
	unsigned long nslabs;        /* # of slabs allocated here */
};

/********* memory header operations *************/
static inline struct ps_mheader *
__ps_mhead_get(void *mem)
{ return (struct ps_mheader *)((char*)mem - sizeof(struct ps_mheader)); }

static inline void *
__ps_mhead_mem(struct ps_mheader *h)
{ return &h[1]; }

static inline int
__ps_mhead_isfree(struct ps_mheader *h)
{
	return (h->type == SLAB_FREE);
	return (h->type != SLAB_IN_USE);
}

static inline void
__ps_mhead_setfree(struct ps_mheader *h)
{
	h->type = SLAB_FREE;
	//h->type = bi_global_rtdsc();
	bi_wb_cache(h);
}

static inline void
__ps_mhead_reset(struct ps_mheader *h)
{
	h->next = NULL;
	//h->type = SLAB_IN_USE;
	h->type = bi_global_rtdsc();
	bi_wb_cache(h);
}

static inline void
__ps_mhead_init(struct ps_mheader *h, struct ps_slab *s)
{
	h->slab = s;
	h->type = SLAB_IN_USE;
}

/*********** some slab internals **/
static inline unsigned long
__ps_slab_objmemsz(size_t obj_sz)
{ return PS_RNDUP(obj_sz + sizeof(struct ps_mheader), CACHE_LINE); }
static inline unsigned long
__ps_slab_max_nobjs(size_t obj_sz, size_t allocsz, size_t headoff)
{ return (allocsz - headoff) / __ps_slab_objmemsz(obj_sz); }
/* The offset of the given object in its slab */
static inline unsigned long
__ps_slab_objsoff(struct ps_slab *s, struct ps_mheader *h, size_t obj_sz, size_t headoff)
{ return ((unsigned long)h - ((unsigned long)s->memory + headoff)) / __ps_slab_objmemsz(obj_sz); }

/*********** slab apis ************/
static inline int
bi_slab_isempty(struct ps_slab_info *si)
{ return si->nslabs; }

static inline size_t
bi_slab_objmem(struct ps_slab_info *si)
{ return __ps_slab_objmemsz(si->obj_sz); }

static inline size_t
bi_slab_nobjs(struct ps_slab_info *si)
{ return __ps_slab_max_nobjs(si->obj_sz, MEM_MGR_OBJ_SZ, sizeof(struct ps_slab)); }

static inline unsigned long
bi_slab_objoff(struct ps_slab_info *si, void *obj)
{
	struct ps_mheader *h = __ps_mhead_get(obj);
	return __ps_slab_objsoff(h->slab, h, si->obj_sz, sizeof(struct ps_slab));
}

struct ps_slab_info *bi_slab_create(size_t obj_sz);
void *bi_slab_alloc(struct ps_slab_info *si);
void bi_slab_free(void *buf);

#endif /* PS_SLAB_H */
