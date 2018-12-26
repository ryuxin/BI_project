/*
 *  Concurrent balanced trees
 */

#ifndef	_LINUX_CBTREE_H
#define	_LINUX_CBTREE_H

#include <pthread.h>
#include "bi.h"

// Sequential operations
#define SHARED(typ, name) struct { typ val; } name
#define SET(sh, v) ((sh).val = (v))
#define GET(sh) ((const __typeof__((sh).val)) (sh).val)
#define V(x) ({typeof(x) __x = (x); __x ? __x->value : NULL;})

/******************************************************************
 * Tree types
 */

struct cb_kv {
	uintptr_t key;
	void *value;
};
typedef uintptr_t k_t;
typedef struct cb_kv kv_t;
struct TreeBB_Node {
	SHARED(struct TreeBB_Node *, left);
	SHARED(struct TreeBB_Node *, right);
	SHARED(unsigned int, size);
	kv_t kv;
};
struct cb_root {
	struct TreeBB_Node *root;
};
typedef struct TreeBB_Node node_t;
enum { INPLACE = 1 };
enum { WEIGHT = 4 };

struct thread_data {
	int nd, cd, ncore, range;
	struct cb_root *root;
	int nread, nupdate;
	uint64_t rtsc, wtsc, tot_tsc;
} __attribute__((aligned(CACHE_LINE), packed));

extern struct ps_slab_info *slab_allocator;
extern struct thread_data tds[NUM_CORE_PER_NODE];

static inline void
__init_thd_data(int nd, int cd, int ncore, int range, struct cb_root *root)
{
	memset(&tds[cd], 0, sizeof(struct thread_data));
	tds[cd].nd    = nd;
	tds[cd].cd    = cd;
	tds[cd].ncore = ncore;
	tds[cd].range = range;
	tds[cd].root  = root;
}

static inline node_t*
nodeOf(kv_t *kv)
{
	        return (node_t*)((char*)kv - offsetof(node_t, kv));
}

static inline int
nodeSize(node_t *node)
{
	        return node ? GET(node->size) : 0;
}

static inline struct cb_root *
get_rb_root(int id)
{
	return (struct cb_root *)get_test_obj(id);
}

void TreeBB_Insert(struct cb_root *tree, uintptr_t key, void *value);
void *TreeBB_Delete(struct cb_root *tree, uintptr_t key);
struct cb_kv *TreeBB_Find(struct cb_root *tree, uintptr_t needle);

/******** arch dependent *******/
node_t *TreeBBNewNode(void);
void TreeBBFreeNode(node_t *n);
void cb_insert(struct cb_root *tree, uintptr_t key, void *value);
void *cb_erase(struct cb_root *tree, uintptr_t key);
struct cb_kv *cb_find(struct cb_root *tree, uintptr_t needle);
void cb_tree_init(struct cb_root *tree, int tree_sz, int range);
void spawn_writer(pthread_t *thd, int nd, int cd);
void spawn_reader(pthread_t *thd, int nd, int cd);
void join_wirter(pthread_t thd);
#ifdef RBTREE_TEST
void test_correct(struct thread_data *mythd);
#endif

#endif	/* _LINUX_CBTREE_H */
