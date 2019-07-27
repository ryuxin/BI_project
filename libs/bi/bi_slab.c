#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mem_mgr.h"
#include "bi_slab.h"

/*** Operations on the freelist of slabs ***/
#ifdef PS_SLAB_DEBUG
static inline void
__ps_slab_check_consistency(struct ps_slab *s)
{
	struct ps_mheader *h;
	unsigned int i;

	assert(s);
	h = s->freelist;
	for (i = 0 ; h ; i++) {
		assert(h->slab == s);
		h = h->next;
	}
	assert(i == s->nfree);
}

static inline void
__ps_slab_freelist_check(struct ps_slab_freelist *fl)
{
	struct ps_slab *s = fl->list;
	
	if (!s) return;
	do {
		assert(s->memory && s->freelist);
		assert(ps_list_prev(ps_list_next(s, list), list) == s);
		assert(ps_list_next(ps_list_prev(s, list), list) == s);
		__ps_slab_check_consistency(s);
	} while ((s = ps_list_first(s, list)) != fl->list);
}
#else  /* PS_SLAB_DEBUG */
static inline void __ps_slab_check_consistency(struct ps_slab *s) { (void)s; }
static inline void __ps_slab_freelist_check(struct ps_slab_freelist *fl) { (void)fl; }
#endif /* PS_SLAB_DEBUG */

static void
__slab_freelist_rem(struct ps_slab_freelist *fl, struct ps_slab *s)
{
	assert(s && fl);
	if (fl->list == s) {
		if (ps_list_singleton(s, list)) fl->list = NULL;
		else                        fl->list = ps_list_next(s, list);
	}
	ps_list_rem(s, list);
}

static void
__slab_freelist_add(struct ps_slab_freelist *fl, struct ps_slab *s)
{
	assert(s && fl);
	assert(ps_list_singleton(s, list));
	assert(s != fl->list);
	if (fl->list) ps_list_add(fl->list, s, list);
	fl->list = s;
	/* TODO: sort based on emptiness...just use N bins */
}

static void
__ps_slab_init(struct ps_slab_info *si, struct ps_slab *s, size_t allocsz, size_t headoff)
{
	size_t nfree, i;
	size_t objmemsz  = __ps_slab_objmemsz(si->obj_sz);
	struct ps_mheader *alloc, *prev;

	s->nfree = nfree = (allocsz - headoff) / objmemsz;
	s->memsz = allocsz;
	s->si    = si;
	alloc    = (struct ps_mheader *)((char *)s->memory + headoff);
	prev     = s->freelist = alloc;
	for (i = 0 ; i < nfree ; i++, prev = alloc, alloc = (struct ps_mheader *)((char *)alloc + objmemsz)) {
		__ps_mhead_init(alloc, s);
		prev->next = alloc;
	}
	/* better not overrun memory */
	assert((void *)alloc <= (void *)((char*)s->memory + allocsz));

	ps_list_init(s, list);
	__slab_freelist_add(&si->fl, s);
	__ps_slab_freelist_check(&si->fl);
}

static struct ps_slab *
ps_slab_defalloc(size_t sz)
{
	struct Free_mem_item *mem_item;
	struct ps_slab *s;

	mem_item    = mem_mgr_alloc(sz);
	s           = mem_item->addr;
	assert(s);
	s->memory   = s;
	s->mem_item = mem_item;
	return s;
}

static void
ps_slab_deffree(struct ps_slab *s)
{
	mem_mgr_free(s->mem_item);
}

static inline void
__ps_slab_mem_free(void *buf, size_t allocsz, size_t headoff)
{
	struct ps_slab *s;
	struct ps_mheader *h, *next;
	struct ps_slab_info *si;
	struct ps_slab_freelist *fl, *el;
	unsigned int max_nobjs;

	h = __ps_mhead_get(buf);
	s = h->slab;
	assert(s);
	si = s->si;
	assert(si);
	assert(__ps_slab_objmemsz(si->obj_sz) + headoff <= allocsz);
	assert(si->nodeid == NODE_ID());
	max_nobjs = __ps_slab_max_nobjs(si->obj_sz, allocsz, headoff);

	next        = s->freelist;
	s->freelist = h; 	/* TODO: should be atomic/locked */
	h->next     = next;
//	__ps_mhead_setfree(h);
	s->nfree++;		/* TODO: ditto */
	if (s->nfree == max_nobjs) {
		/* remove from the freelist */
		__slab_freelist_rem(&si->fl, s);
		si->nslabs--;
	 	ps_slab_deffree(s);
	} else if (s->nfree == 1) {
		fl = &si->fl;
		el = &si->el;
		__slab_freelist_rem(el, s);
		/* add back onto the freelists */
		assert(ps_list_singleton(s, list));
		assert(s->memory && s->freelist);
		__slab_freelist_add(fl, s);
	}
	__ps_slab_freelist_check(&si->fl);

	return;
}

static inline void *
__ps_slab_mem_alloc(struct ps_slab_info *si, size_t allocsz, size_t headoff)
{
	struct ps_slab      *s;
	struct ps_mheader   *h;
	assert(si->obj_sz + headoff <= allocsz);

	s = si->fl.list;
	if (bi_unlikely(!s)) {
		/* allocation function must initialize s->memory */
		s = ps_slab_defalloc(allocsz);
		if (bi_unlikely(!s)) return NULL;
		
		__ps_slab_init(si, s, allocsz, headoff);
		si->nslabs++;
		assert(s->memory && s->freelist);
	}

	assert(s && s->freelist);
	/* TODO: atomic modification to the freelist */
	h           = s->freelist;
	s->freelist = h->next;
	s->nfree--;
	__ps_mhead_reset(h);

	/* remove from the freelist */
	if (s->nfree == 0) {
		__slab_freelist_rem(&si->fl, s);
		assert(ps_list_singleton(s, list));
		__slab_freelist_add(&si->el, s);
	}
	__ps_slab_freelist_check(&si->fl);
	dbg_log_add("alloc mem", h);

	return __ps_mhead_mem(h);
}

static inline void
bi_slab_init(struct ps_slab_info *si, size_t obj_sz)
{
	memset(si, 0, sizeof(struct ps_slab_info));
	si->obj_sz = obj_sz;
	si->nodeid = NODE_ID();
}

struct ps_slab_info *
bi_slab_create(size_t obj_sz)
{
	struct ps_slab_info *si;
	assert(__ps_slab_objmemsz(obj_sz) < MEM_MGR_OBJ_SZ);
	printf("dbg slab sz %ld\n", __ps_slab_objmemsz(obj_sz));

	si = malloc(sizeof(struct ps_slab_info));
	bi_slab_init(si, obj_sz);
	return si;
}

void *
bi_slab_alloc(struct ps_slab_info *si)
{
	void *ret;
	assert(si->nodeid == NODE_ID());
	ret = __ps_slab_mem_alloc(si, MEM_MGR_OBJ_SZ, sizeof(struct ps_slab));
	return ret;
}

void
bi_slab_free(void *buf)
{
	assert(buf);
	__ps_slab_mem_free(buf, MEM_MGR_OBJ_SZ, sizeof(struct ps_slab));
}
