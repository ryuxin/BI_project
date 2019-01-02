#ifdef ATOMIC_OBJ_RCU

#include <stdio.h>
#include <stdlib.h>
#include <urcu.h>
#include "bi_rcu.h"
#include "atomic_obj.h"

#define RCU_FREE_BUF_SZ 1024

__thread void *local_buf[RCU_FREE_BUF_SZ];
__thread int local_h = 0;

extern void rcu_bi_init(void);

static inline int
__rcu_free(void *buf)
{
	int i;

	if (local_h == RCU_FREE_BUF_SZ) {
		synchronize_rcu();
		for(i=0; i<RCU_FREE_BUF_SZ; i++) bi_free(local_buf[i]);
		local_h = 0;
	}
	local_buf[local_h++] = buf;
	return local_h;
}

void
atomic_obj_init(int num, size_t sz)
{
	int i;
	struct Test_obj *to;

	if (!NODE_ID()) bi_rcu_init_global((struct RCU_block *)get_rcu_area());
	bi_rcu_init_local();
	rcu_bi_init();
	if (NODE_ID()) return ;
	assert(num <= MAX_TEST_OBJ_NUM);
	for(i=0; i<num; i++) {
		to       = get_test_obj(i);
		to->sz   = sz;
		to->data = bi_malloc(sz);
	}
}

void
atomic_obj_read(int id)
{
	struct Test_obj *to;

	rcu_read_lock();
	to = get_test_obj(id);
	assert(to->data);
	memcpy(temp_obj, to->data, to->sz);
	rcu_read_unlock();
}

void
atomic_obj_write(int id)
{
	struct ck_spinlock_mcs **lock;
	ck_spinlock_mcs_t cntxt;
	void *new_data, *old;
	struct Test_obj *to;

	to       = get_test_obj(id);
	lock     = get_mcs_lock(id);
	cntxt    = get_mcs_lock_cntxt();
	new_data = bi_malloc(to->sz);
	assert(new_data);
	ck_spinlock_mcs_lock(lock, cntxt);
	old      = rcu_dereference(to->data);
	assert(to->data);
	memcpy(new_data, temp_obj, to->sz);
	rcu_assign_pointer(to->data, new_data);
	ck_spinlock_mcs_unlock(lock, cntxt);
	__rcu_free(old);
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
