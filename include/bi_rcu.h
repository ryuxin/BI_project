#include <ck_spinlock.h>
#include <urcu.h>
#include "constant.h"

struct RCU_wait_node {
	struct urcu_wait_node w;;
	char pad[CACHE_LINE_PAD(sizeof(struct urcu_wait_node))];
};

struct RCU_ctr {
	unsigned long ctr;
	char pad[CACHE_LINE_PAD(sizeof(unsigned long))];
};

struct RCU_block {
	ck_spinlock_mcs_t gp_lock;
	char pad1[CACHE_LINE_PAD(sizeof(ck_spinlock_mcs_t))];
	struct urcu_wait_queue gp_wait;
	char pad2[CACHE_LINE_PAD(sizeof(struct urcu_wait_queue))];
	struct urcu_gp global_gp;
	struct RCU_wait_node waits[NUM_NODES][NUM_CORE_PER_NODE];
	struct RCU_ctr ctrs[NUM_NODES][NUM_CORE_PER_NODE];
};

void bi_rcu_init_global(struct RCU_block *rb);
void bi_rcu_init_local(void *);
