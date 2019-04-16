#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bi_smr.h"

#define LOG_N 20000000

struct qsc_local_cache {
	void **m;
	size_t *sz;
	int cnt;
};

static struct qsc_local_cache smr_cache, wlog_cache;
struct parsec parsec_time_cache[NUM_CORE_PER_NODE];

int dbgf = 0;
static void **logm = NULL;

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
qsc_ring_enqueue_batch(struct bi_qsc_ring *ql, void **m, size_t *sz, int batch)
{
	int i, j;
	struct quies_item *qi;

	for(j = ql->tail, i=0; i<batch; i++) {
		if (ql->head == (j + 1) % MAX_QUI_RING_LEN) {
			printf("quisence ring full %d\n", MAX_QUI_RING_LEN);
			assert(0);
			return -1;
		}

	        qi           = &(ql->ring[j]);
        	qi->mh       = m[i];
	        qi->sz       = sz[i];
        	qi->tsc_free = bi_global_rtdsc();
	        clwb_range_opt(qi, CACHE_LINE);
		j = (j + 1) % MAX_QUI_RING_LEN;
	}
	bi_wmb();
	ql->tail = j;
	clwb_range_opt(ql, CACHE_LINE);
	return 0;
}

static inline void
qsc_local_cache_init(struct qsc_local_cache *lc)
{
        lc->cnt = 0;
}

static inline void
qsc_local_cache_alloc(struct qsc_local_cache *lc)
{
        lc->m  = malloc(sizeof(void *) * LOCAL_CACHE_QUEUE_SZ);
        lc->sz = malloc(sizeof(size_t) * LOCAL_CACHE_QUEUE_SZ);
        qsc_local_cache_init(lc);
}

static inline void
qsc_local_cache_put(struct qsc_local_cache *lc, void *buf, size_t s)
{
	int c;

	c = lc->cnt;
	assert(c < LOCAL_CACHE_QUEUE_SZ);
	lc->m[c]  = buf;
	lc->sz[c] = s;
	lc->cnt   = c+1;
}

static inline int
qsc_local_cache_flush(struct bi_qsc_ring *ql, struct qsc_local_cache *lc)
{
	int r;
	r = lc->cnt;
	qsc_ring_enqueue_batch(ql, lc->m, lc->sz, lc->cnt);
	qsc_local_cache_init(lc);
	return r;
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

void
bi_wlog_cache_init(void)
{
        qsc_local_cache_init(&wlog_cache);
}

/*
 * core API of BI:
 * A global shared ring buffer is the central to coordinate and achieve quiescence.
 * Only writer add and remove memory objects from the ring buffer via *_free and 
 * *_reclaim API. The reclamation likely needs another *_quiesce API to detect 
 * if an objects can be safely removed (based on time and quiescence calculation).
 * So time-stamp is recorded when object is added to the ring buffer.
 * Readers use *_flush API to do cache flush of all objects in the ring buffer.
 * This process first flush the ring buffer itself in order to see up-to-date 
 * entries, then flush each object stored in the entry.
 * To avoid frequent access to the global ring buffer (its associated write back 
 * and memory barriers), the writer has a local cache which save objects temporarily 
 * and add them to the global ring buffer later in a batched manner.
 * The same set of API is used for ring buffer of both freed memory and modification log.
 */
void
bi_qsc_cache_init(void)
{
	qsc_local_cache_init(&smr_cache);
        qsc_local_cache_init(&wlog_cache);
}

void
bi_qsc_cache_alloc(void)
{
	qsc_local_cache_alloc(&smr_cache);
#ifdef ENABLE_WLOG
        qsc_local_cache_alloc(&wlog_cache);
#endif
}

int
bi_qsc_cache_flush(void)
{
	int r;
	r = qsc_local_cache_flush(get_quies_ring(NODE_ID()), &smr_cache);
#ifdef ENABLE_WLOG
	r += qsc_local_cache_flush(get_wlog_ring(NODE_ID()), &wlog_cache);
#endif
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

void
bi_smr_free(void *buf)
{
        struct ps_mheader *m;
	size_t sz;

        assert(buf);
        m  = __ps_mhead_get(buf);
	sz = bi_slab_objmem(m->slab->si); 
	__ps_mhead_setfree(m);
//	qsc_ring_enqueue_batch(get_quies_ring(NODE_ID()), &m, &sz, 1);
        qsc_local_cache_put(&smr_cache, m, sz);
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
bi_smr_flush(void)
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

void
bi_wlog_free(void *buf, size_t sz)
{
#ifdef ENABLE_WLOG
        assert(buf);
//	qsc_ring_enqueue_batch(get_wlog_ring(NODE_ID()), &buf, &sz, 1);
        qsc_local_cache_put(&wlog_cache, buf, sz);
#else
        (void)buf;
        bi_mb();
#endif
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

int
bi_wlog_flush(void)
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

#if 0 
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
	clwb_range_opt(ps, sizeof(struct parsec));
}

void
bi_time_flush(void)
{ return ; }
#else
void
bi_enter(void)
{
	struct parsec *ps;
	ps_tsc_t curr_time;

	curr_time    = bi_global_rtdsc();
	ps           = &parsec_time_cache[CORE_ID()];
	ps->time_in  = curr_time;
	ps->time_out = curr_time - 1;
	bi_wmb();
}

void
bi_exit(void)
{
	struct parsec *ps;

	ps           = &parsec_time_cache[CORE_ID()];
	ps->time_out = ps->time_in + 1;
}

void
bi_time_flush(void)
{
	void *ps;
	size_t sz;

	ps = get_parsec_time(NODE_ID(), 0);
	sz = sizeof(parsec_time_cache);
	memcpy(ps, parsec_time_cache, sz);
	clwb_range_opt(ps, sz);
}
#endif


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
