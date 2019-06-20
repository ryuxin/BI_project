/***
 * A Scalable Memory Reclamation (SMR) technique built off of the slab
 * allocator for parsec (parallel sections).  Maintains a freelist per
 * slab with memory items ordered in terms of the Time Stamp Counter
 * (tsc) taken when the node was freed.  Removal from these queues is
 * governed by quiescence of parallel threads at the time the memory
 * was freed (which might be some time in the past).  This code
 * specifies the policy for when memory flows between the quiescing
 * queues, and the slab memory.  Moving memory back to the slabs is
 * important to enable us to reclaim and migrate memory between cores
 * (each slab is owned by a core), thus there is some balancing to be
 * done here.
 */

#ifndef PS_SMR_H
#define PS_SMR_H

#include "mem_mgr.h"
#include "bi_slab.h"

struct parsec {
	volatile ps_tsc_t time_in, time_out;
} __attribute__((aligned(CACHE_LINE), packed));

struct quies_item {
	void *mh;
	size_t sz;
	uint64_t tsc_free;
} __attribute__((packed));

struct bi_qsc_ring {
	int head, tail;
	char pad[CACHE_LINE - 2*sizeof(int)];
	struct quies_item ring[MAX_QUI_RING_LEN];
} __attribute__((aligned(CACHE_LINE), packed));

extern struct parsec parsec_time_cache[NUM_CORE_PER_NODE];

static inline void
parsec_struct_init(struct parsec *p)
{
	p->time_in = 0;
	p->time_out = 1;
}

static inline void
qsc_ring_struct_init(struct bi_qsc_ring *p)
{
	p->head = p->tail = 0;
}

void bi_wlog_cache_init(void);
void bi_qsc_cache_init(void);
void bi_qsc_cache_alloc(void);
int bi_qsc_cache_flush(void);

uint64_t bi_quiescei_smr(uint64_t);
uint64_t bi_quiesce_cache(uint64_t);
uint64_t bi_quiesce(uint64_t);

void bi_smr_free(void *buf);
int bi_smr_reclaim(void);
int bi_smr_flush(void);

void bi_wlog_free(void *buf, size_t sz);
int bi_wlog_reclaim(void);
int bi_wlog_flush(void);

void bi_enter(void);
void bi_exit(void);
void bi_time_flush(void);

void chklog(void *v);

#endif	/* PS_SMR_H */
