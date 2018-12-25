#ifdef ATOMIC_OBJ_LOCK

#include "atomic_obj.h"

struct ps_slab_info *slab_allocator;

void
atomic_obj_init(int num, size_t sz)
{
	int i;
	struct Test_obj *to;

	slab_allocator = bi_slab_create(sz);
	assert(num <= MAX_TEST_OBJ_NUM);
	for(i=0; i<num; i++) {
		to       = get_test_obj(i);
		to->sz   = sz;
		to->data = bi_slab_alloc(slab_allocator);
	}
	temp_obj = malloc(sz);
	memset(temp_obj, '$', sz);
}

void
atomic_obj_read(int id)
{
	struct ck_spinlock_mcs **lock;
	ck_spinlock_mcs_t cntxt;
	struct Test_obj *to;

	to    = get_test_obj(id);
	lock  = get_mcs_lock(id);
	cntxt = get_mcs_lock_cntxt();
	ck_spinlock_mcs_lock(lock, cntxt);
	assert(to->data);
	memcpy(temp_obj, to->data, to->sz);
	ck_spinlock_mcs_unlock(lock, cntxt);
}

void
atomic_obj_write(int id)
{
	struct ck_spinlock_mcs **lock;
	ck_spinlock_mcs_t cntxt;
	struct Test_obj *to;

	to    = get_test_obj(id);
	lock  = get_mcs_lock(id);
	cntxt = get_mcs_lock_cntxt();
	ck_spinlock_mcs_lock(lock, cntxt);
	assert(to->data);
	memcpy(to->data, temp_obj, to->sz);
	ck_spinlock_mcs_unlock(lock, cntxt);
}

void
spawn_writer(pthread_t thd, int nd, int cd, int ncore)
{
	spawn_reader(thd, nd, cd, ncore);
}

#endif
