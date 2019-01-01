#ifdef RBTREE_BI

#include <stdio.h>
#include <stdlib.h>
#include "cbtree.h"

#define MAX_FREE_BUF_SZ (1024)

/************ TODO add app flush function only flush root? ********/

struct rbtree_msg {
	struct cb_root *tree;
	uintptr_t key;
	void *value;
	int type; /* 0 insert, 1 delete, -1 is reply */
};

static int totalFreed = 0;
static node_t *free_buf[MAX_FREE_BUF_SZ];
struct rbtree_msg msg_reply = { .type = -1 };

node_t *
TreeBBNewNode(void)
{
	return bi_slab_alloc(slab_allocator);
}

void
TreeBBFreeNode(node_t *n)
{
	assert(totalFreed < MAX_FREE_BUF_SZ);
	free_buf[totalFreed++] = n;
}

static void
free_nodes(void)
{
	for(; totalFreed>0; totalFreed--) {
		bi_smr_free(free_buf[totalFreed - 1]);
	}
}

static void
cb_insert_svr(struct cb_root *tree, uintptr_t key, void *value)
{
	TreeBB_Insert(tree, key, value);
}

static void *
cb_erase_svr(struct cb_root *tree, uintptr_t key)
{
	return TreeBB_Delete(tree, key);
}

void
rbtree_msg_handler(void *msg, size_t sz, int nid, int cid)
{
	struct rbtree_msg *req;
	int r;

	assert(msg);
	assert(sz == sizeof(struct rbtree_msg));

	req = (struct rbtree_msg *)msg;
	assert(req->type >= 0);
	if (req->type == 0) cb_insert_svr(req->tree, req->key, req->value);
	else msg_reply.value = cb_erase_svr(req->tree, req->key);
	r = rpc_send_server(nid, cid, &msg_reply, sz);
	free_nodes();
	assert(r == 0);
}

static void *
writer_thd_fn(void *arg)
{
	struct thread_data *mythd;

	mythd = (struct thread_data *)arg;
	thd_set_affinity(pthread_self(), mythd->nd, mythd->cd);
	bi_local_init_server(mythd->cd, mythd->ncore);
	bi_server_run(rbtree_msg_handler, NULL);
	return NULL;
}

static void
rbtree_send_msg(struct rbtree_msg *msg)
{
	int r;
	size_t s;

	r = rpc_send(0, msg, sizeof(struct rbtree_msg));
	assert(r == 0);
	s = rpc_recv(0, msg, 1);
	assert(s == sizeof(struct rbtree_msg));
	assert(msg->type == -1);
}

void
cb_insert(struct cb_root *tree, uintptr_t key, void *value)
{
	struct rbtree_msg msg;

	msg.tree  = tree;
	msg.key   = key;
	msg.value = value;
	msg.type  = 0;
	rbtree_send_msg(&msg);
}

void *
cb_erase(struct cb_root *tree, uintptr_t key)
{
	struct rbtree_msg msg;

	msg.tree  = tree;
	msg.key   = key;
	msg.type  = 1;
	rbtree_send_msg(&msg);
	return msg.value;
}

struct cb_kv *
cb_find(struct cb_root *tree, uintptr_t needle)
{
	struct cb_kv *ret;

	bi_enter();
	ret = TreeBB_Find(tree, needle);
	bi_exit();
	return ret;
}

void
cb_tree_init(struct cb_root *tree, int tree_sz, int range)
{
	long i, j, r;
	tree->root = NULL;
	r = range / tree_sz;
	for(j=0, i=0; i<tree_sz; i++) {
		cb_insert_svr(tree, (k_t)j, (void *)j);
		j = (j + r) % range;
	}
	assert(nodeSize(tree->root) == tree_sz);
}

void
spawn_writer(pthread_t *thd, int nd, int cd)
{
	int ret;

	ret = pthread_create(thd, 0, writer_thd_fn, &tds[cd]);
	if (ret) {
		printf("create writer nd %d cd %d failed\n", nd, cd);
		exit(-1);
	}
}

void
join_wirter(pthread_t thd)
{ (void)thd; return ; }

#endif
