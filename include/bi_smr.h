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

static inline void
parsec_struct_init(struct parsec *p)
{
	p->time_in = 1;
	p->time_out = 0;
}

static inline void
qsc_ring_struct_init(struct bi_qsc_ring *p)
{
	p->head = p->tail = 0;
}

int bi_smr_flush_wlog(void);
void bi_smr_wlog(void *buf);
int bi_wlog_reclaim(void);
int bi_smr_flush(void);
uint64_t bi_quiesce(void);
int bi_smr_reclaim(void);
void bi_smr_free(void *buf);
void bi_enter(void);
void bi_exit(void);

void chklog(void *v);

#endif	/* PS_SMR_H */
