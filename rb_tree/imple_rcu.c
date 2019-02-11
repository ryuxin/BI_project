#ifdef RBTREE_URCU

#include <stdio.h>
#include <stdlib.h>
#include <urcu.h>
#include "bi_rcu.h"
#include "cbtree.h"

#define MAX_FREE_BUF_SZ (1024)
#define RCU_FREE_BATCH (MAX_FREE_BUF_SZ/2)

__thread int totalFreed = 0;
__thread node_t *free_buf[MAX_FREE_BUF_SZ];
static struct ck_spinlock_mcs **lock;

extern void rcu_bi_init(void);

node_t *
TreeBBNewNode(void)
{
	node_t *r;
	r = bi_malloc(sizeof(node_t));
	assert(r);
	return r;
}

void
TreeBBFreeNode(node_t *n)
{
	assert(totalFreed < MAX_FREE_BUF_SZ);
	free_buf[totalFreed++] = n;
}

static inline void
free_nodes_seq(void)
{
        for(; totalFreed>0; totalFreed--) {
                bi_free(free_buf[totalFreed - 1]);
	}
}

static void
free_nodes(void)
{
	if (totalFreed < RCU_FREE_BATCH) return ;
	synchronize_rcu();
	for(; totalFreed>0; totalFreed--) {
		bi_free(free_buf[totalFreed - 1]);
	}
}

void
cb_insert(struct cb_root *tree, uintptr_t key, void *value)
{
	ck_spinlock_mcs_t cntxt;

	cntxt = get_mcs_lock_cntxt();
	ck_spinlock_mcs_lock(lock, cntxt);
	TreeBB_Insert(tree, key, value);
	ck_spinlock_mcs_unlock(lock, cntxt);
	free_nodes();
}

void *
cb_erase(struct cb_root *tree, uintptr_t key)
{
	void *r;
	ck_spinlock_mcs_t cntxt;

        cntxt = get_mcs_lock_cntxt();
	ck_spinlock_mcs_lock(lock, cntxt);
	r = TreeBB_Delete(tree, key);
	ck_spinlock_mcs_unlock(lock, cntxt);
	free_nodes();
	return r;
}

struct cb_kv *
cb_find(struct cb_root *tree, uintptr_t needle)
{
	struct cb_kv *ret;

	rcu_read_lock();
	ret = TreeBB_Find(tree, needle);
	if (ret) assert(V(ret) == (void *)needle);
	rcu_read_unlock();
	return ret;
}

void
cb_tree_init(struct cb_root *tree, int tree_sz, int range)
{
	long i, j, r;

	lock  = get_mcs_lock(0);
	if (!NODE_ID()) bi_rcu_init_global((struct RCU_block *)get_rcu_area());
	bi_rcu_init_local();
	rcu_bi_init();
        if (NODE_ID()) return ;
	tree->root = NULL;
	r = range / tree_sz;
	for(j=0, i=0; i<tree_sz; i++) {
		cb_insert(tree, (k_t)j, (void *)j);
		free_nodes_seq();
		j = (j + r) % range;
	}
	assert(nodeSize(tree->root) == tree_sz);
}

void
spawn_writer(pthread_t *thd, int nd, int cd)
{
	spawn_reader(thd, nd, cd);
}

void
join_wirter(pthread_t thd)
{
        int ret;
        pthread_join(thd, (void *)&ret);
}

#endif
