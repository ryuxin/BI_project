#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bi_smr.h"

#define LOG_N 20000000
int dbgf = 0;
static void **logm = NULL;
static void
logwf(void *v)
{
	if (!logm) logm = malloc(sizeof(void *) * LOG_N);
	if (dbgf >= LOG_N) return ;
	logm[dbgf++] = v;
}

void
chklog(void *v)
{
	int i;
	printf("dbg tot log %d\n", dbgf);
	for(i=0; i<dbgf; i++) {
		if (logm[i] == v) printf("dbg e flush %p v %p\n", logm[i], v);
	}
}

static inline int
qsc_ring_len(struct bi_qsc_ring *ql)
{
	return (ql->tail + MAX_QUI_RING_LEN - ql->head) % MAX_QUI_RING_LEN;
}

static inline struct quies_item *
qsc_ring_peek(struct bi_qsc_ring *ql)
{
	if (ql->head == ql->tail) return NULL;
	return &(ql->ring[ql->head]);
}

static inline struct quies_item *
qsc_ring_dequeue(struct bi_qsc_ring *ql)
{
	struct quies_item *ret;

	if (ql->head == ql->tail) return NULL;
	ret      = &(ql->ring[ql->head]);
	ql->head = (ql->head+1) % MAX_QUI_RING_LEN;
	return ret;
}

static inline int
qsc_ring_enqueue(struct bi_qsc_ring *ql, void *m, size_t sz)
{
	struct quies_item *qi;

	if (ql->head == (ql->tail + 1) % MAX_QUI_RING_LEN) {
		printf("quisence ring full %d\n", MAX_QUI_RING_LEN);
		assert(0);
		return -1;
	}
	qi           = &(ql->ring[ql->tail]);
	qi->mh       = m;
	qi->sz       = sz;
	qi->tsc_free = bi_global_rtdsc();
	clwb_range(qi, CACHE_LINE);
	ql->tail     = (ql->tail+1) % MAX_QUI_RING_LEN;
	clwb_range(ql, CACHE_LINE);
	return 0;
}

static inline int
qsc_enqueue(struct bi_qsc_ring *ql, struct ps_mheader *m)
{
	return qsc_ring_enqueue(ql, m, bi_slab_objmem(m->slab->si));
}

static inline void
qsc_ring_flush(struct bi_qsc_ring *ql)
{
#define FETCH_LEN 32
	int i, j;
	struct quies_item *s;

	for(i=ql->head; i != ql->tail; i = (i+1) % MAX_QUI_RING_LEN) {
		s = &(ql->ring[i]);
		if (i % FETCH_LEN == 0) {
			j = (i + FETCH_LEN) % MAX_QUI_RING_LEN;
			__builtin_prefetch(&(ql->ring[j]), 0, 0);
		}
		clflush_range_opt(s->mh, s->sz);
	}
}

static inline int
__ps_in_lib(struct parsec *ps)
{ return ps->time_out <= ps->time_in; }

static int
bi_smr_flush_quiesce_queue(void)
{
	int i, r, qsc_cpu, tot_cpu, curr;
	struct bi_qsc_ring *ql;

	curr    = NODE_ID();
	tot_cpu = get_active_node_num();
	for (i = 1 ; i < tot_cpu; i++) {
		/* Make sure we don't all hammer core 0... */
		qsc_cpu = (curr + i) % tot_cpu;
		assert(qsc_cpu != curr);
		ql = get_quies_ring(qsc_cpu);
		assert(ql);
		clflush_range_opt(ql, sizeof(struct bi_qsc_ring));
	}
	bi_mb();

	for (r=0, i = 0; i < tot_cpu; i++) {
		qsc_cpu = (curr + i) % tot_cpu;
		ql = get_quies_ring(qsc_cpu);
		r += (MAX_QUI_RING_LEN + ql->tail - ql->head) % MAX_QUI_RING_LEN;
		qsc_ring_flush(ql);
	}
	bi_mb();
	return r;
}

int
bi_smr_flush_wlog(void)
{
        int i, r, qsc_cpu, tot_cpu, curr;
        struct bi_qsc_ring *ql;

#ifndef ENABLE_WLOG
	printf("Warning writer log is not enabled\n");
	return -1;
#endif

	curr    = NODE_ID();
        tot_cpu = get_active_node_num();
        for (i = 1 ; i < tot_cpu; i++) {
		/* Make sure we don't all hammer core 0... */
		qsc_cpu = (curr + i) % tot_cpu;
                assert(qsc_cpu != curr);
                ql = get_wlog_ring(qsc_cpu);
                clflush_range_opt(ql, sizeof(struct bi_qsc_ring));
        }
	bi_mb();

	for (r=0, i = 1; i < tot_cpu; i++) {
		qsc_cpu = (curr + i) % tot_cpu;
                ql = get_wlog_ring(qsc_cpu);
                r += (MAX_QUI_RING_LEN + ql->tail - ql->head) % MAX_QUI_RING_LEN;
                qsc_ring_flush(ql);
	}
        bi_mb();
	return r;
}

int
bi_smr_flush(void)
{
	int r;
	r = bi_smr_flush_quiesce_queue();
	return r;
}

uint64_t
bi_quiesce(void)
{
	int i, j, qsc_cpu, tot_cpu, curr, tot_core;
	ps_tsc_t min_known_qsc;
	struct parsec t, *remote;

	curr          = NODE_ID();
	tot_cpu       = get_active_node_num();
	tot_core      = get_active_core_num();
	min_known_qsc = bi_global_rtdsc();
	/* invalidate all cores parsec info first */
	for (i = 1 ; i < tot_cpu; i++) {
		/* Make sure we don't all hammer core 0... */
		qsc_cpu = (curr + i) % tot_cpu;
		assert(qsc_cpu != curr);
		for(j=0; j<tot_core; j++) {
			remote = get_parsec_time(qsc_cpu, j);
			clflush_range_opt(remote, sizeof(struct parsec));
		}
	}
	bi_mb();

	for (i = 0; i < tot_cpu; i++) {
		/* Make sure we don't all hammer core 0... */
		qsc_cpu = (curr + i) % tot_cpu;
		for(j=0; j<tot_core; j++) {
			remote = get_parsec_time(qsc_cpu, j);
			memcpy(&t, remote, sizeof(struct parsec));
			if (__ps_in_lib(&t)) {
				if (min_known_qsc > t.time_in) min_known_qsc = t.time_in;
			}
		}
	}
	bi_mb();
	return min_known_qsc;
}

int
bi_smr_reclaim(void)
{
	struct bi_qsc_ring *ql;
	struct quies_item *a;
	int i=0;
	ps_tsc_t qsc;

	ql   = get_quies_ring(NODE_ID());
	assert(ql);
	a    = qsc_ring_peek(ql);
	if (!a) return i;
	qsc  = bi_quiesce();
	qsc -= FLUSH_GRACE_PERIOD;

	while (1) {
		a = qsc_ring_peek(ql);
		if (!a || a->tsc_free > qsc) break;

		a = qsc_ring_dequeue(ql);
		bi_slab_free(__ps_mhead_mem(a->mh));
		i++;
	}
	clwb_range(ql, CACHE_LINE);

	return i;
}

int
bi_wlog_reclaim(void)
{
	struct bi_qsc_ring *ql;
        struct quies_item *a;
        int i=0;
        ps_tsc_t qsc;

	ql   = get_wlog_ring(NODE_ID());
	assert(ql);
        a    = qsc_ring_peek(ql);
	if (!a) return i;
	qsc  = bi_global_rtdsc();
	qsc -= FLUSH_GRACE_PERIOD;
	while (1) {
		a = qsc_ring_peek(ql);
		if (!a || a->tsc_free > qsc) break;
		a = qsc_ring_dequeue(ql);
		i++;
	}
	clwb_range(ql, CACHE_LINE);
	return i;
}

void
bi_smr_wlog(void *buf)
{
#ifdef ENABLE_WLOG
        assert(buf);
        qsc_ring_enqueue(get_wlog_ring(NODE_ID()), buf, CACHE_LINE);
#else
	(void)buf;
	bi_mb();
#endif
}

void
bi_smr_free(void *buf)
{
	struct ps_mheader *m;

	assert(buf);
	m   = __ps_mhead_get(buf);
	qsc_enqueue(get_quies_ring(NODE_ID()), m);
}

void
bi_enter(void)
{
	struct parsec *ps;
	ps_tsc_t curr_time;

	curr_time   = bi_global_rtdsc();
	ps          = get_parsec_time(NODE_ID(), CORE_ID());
	ps->time_in = curr_time;
	/*
	 * The following is needed when we have coarse granularity
	 * time-stamps (i.e. non-cycle granularity, which means we
	 * could have same time-stamp for different events).
	 */
	ps->time_out = curr_time - 1;
	clwb_range(ps, sizeof(struct parsec));
}

void
bi_exit(void)
{
	struct parsec *ps;

	ps = get_parsec_time(NODE_ID(), CORE_ID());
	/*
	 * Here we don't require a full memory barrier on x86 -- only
	 * a compiler barrier is enough.
	 */
	bi_ccb();
	ps->time_out = ps->time_in + 1;
	clwb_range(ps, sizeof(struct parsec));
}
