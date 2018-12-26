#ifdef RBTREE_TEST

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include "cbtree.h"

enum { LEN = 10000, DEL = 50 };

k_t keys[LEN];

/******************************************************************
 * Tree debugging
 */

static void
check(node_t *node, k_t min, k_t max, const k_t *keys, int *pos)
{
	if (!node) return;

	assert(node->kv.key > min);
	assert(node->kv.key < max);
	assert(nodeSize(node) == 1 + nodeSize(GET(node->left)) + nodeSize(GET(node->right)));

	check(GET(node->left), min, node->kv.key, keys, pos);
	assert(node->kv.key == keys[*pos]);
	(*pos)++;
	check(GET(node->right), node->kv.key, max, keys, pos);
}

static void __attribute__((__used__))
show(node_t *node, int depth)
{
	if (!node) return;
	show(GET(node->left), depth + 1);
	printf("%*s%p -> %p\n", depth*2, "", (void*)node->kv.key, node->kv.value);
	show(GET(node->right), depth + 1);
}

static void
TreeBB_Check(struct cb_root *tree, const k_t *keys)
{
	int pos = 0;
	check(tree->root, 0, ~0, keys, &pos);
}

void
test_correct(struct thread_data *mythd)
{
	struct cb_root *tree;
	int i;
	(void)mythd;

	tree = get_rb_root(1);
	for (i = 0; i < LEN; ++i) {
		k_t key = (uintptr_t)i*2;
		printf("+++ %d\n", key);
		cb_insert(tree, key, (void*)key);
		keys[i] = key;
	}
	assert(nodeSize(tree.root) == LEN);
	printf("Insert test passed!");

    for (i = 0; i < DEL; ++i) {
		printf("--- %d\n", keys[i]);
		assert(TreeBB_Delete(tree, keys[i]) == (void*)keys[i]);
		assert(TreeBB_Delete(tree, keys[i]) == NULL);
    }
	assert(nodeSize(tree.root) == LEN-DEL);
	TreeBB_Check(tree, keys+DEL);
	printf("Delete test passed!");

	for (i = DEL; i < LEN; ++i) {
		void *this = (void*)keys[i];
		// XXX Assumes no two keys are consecutive
		assert(V(cb_find(tree, keys[i] - 1)) == NULL);
		assert(V(cb_find(tree, keys[i])) == this);
		assert(V(cb_find(tree, keys[i] + 1)) == NULL);
	}
	printf("Find test passed!");
}

#endif  /* RBTREE_TEST */
