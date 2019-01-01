#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#define _GNU_SOURCE
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <urcu.h>
#include "bi.h"
#include "bi_rcu.h"

struct RCU_block *global_rcu;

static inline void
__gp_wait_init(struct urcu_wait_queue *w)
{
	w->stack.head = CDS_WFS_END;
	w->stack.lock = PTHREAD_MUTEX_INITIALIZER;
}

static inline void
__rcu_gp_init(struct urcu_gp *gp)
{
	gp->ctr = URCU_GP_COUNT;
}

static inline void
__rcu_ctr_init(struct RCU_ctr *c)
{
	c->ctr = 0;
}

struct urcu_gp *
bi_get_gp(void)
{
	return &(global_rcu->global_gp);
}

struct urcu_wait_queue *
bi_get_wait_queue(void)
{
	return &(global_rcu->gp_wait);
}

struct urcu_wait_node *
bi_get_init_wait_node(void)
{
	struct RCU_wait_node *wn;

	wn          = &(global_rcu->waits[NODE_ID()][CORE_ID()]);
	wn->w.state = URCU_WAIT_WAITING;
	return &(wn->w);
}

int
bi_get_nnode(void)
{
	return get_active_node_num();
}

int
bi_get_ncore(void)
{
	return get_active_core_num();
}

unsigned long *
bit_get_crt(int nid, int cid)
{
	return &(global_rcu->ctrs[nid][cid].ctr);
}

unsigned long *
bi_get_self_ctr(void)
{
	return &(global_rcu->ctrs[NODE_ID()][CORE_ID()].ctr);
}

void
bi_gp_lock(void)
{
	ck_spinlock_mcs_lock(global_rcu->gp_lock, get_mcs_lock_cntxt());
}

void
bi_gp_unlock(void)
{
	ck_spinlock_mcs_unlock(global_rcu->gp_lock, get_mcs_lock_cntxt());
}

void
bi_rcu_init_global(struct RCU_block *rb)
{
	int i, j;

	ck_spinlock_mcs_init(&(rb->gp_lock));
	__gp_wait_init(&rb->gp_wait);
	__rcu_gp_init(&(rb->global_gp));
	for(i=0; i<NUM_NODES; i++) {
		for(j=0; j<NUM_CORE_PER_NODE; j++) {
			__rcu_ctr_init(&(rb->ctrs[i][j]));
		}
	}
}

void
bi_rcu_init_local(void *)
{
	global_rcu = (struct RCU_block *)get_rcu_area();
}
