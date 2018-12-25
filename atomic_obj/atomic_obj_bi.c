#ifdef ATOMIC_OBJ_BI

#include "atomic_obj.h"

struct atomic_obj_msg {
	int val; /* >0 is request object id, -1 is reply */
};

struct atomic_obj_msg msg_reply = { .val = -1 };
struct ps_slab_info *slab_allocator;

static inline void
atomic_obj_write_svr(int id)
{
	void *new_data, *old;
	struct Test_obj *to;

	to       = get_test_obj(id);
	old      = bi_dereference_pointer_lazy(&(to->data));
	new_data = bi_slab_alloc();
	assert(to->data);
	memcpy(new_data, temp_obj, to->sz);
	bi_publish_pointer(&(to->data), new_data);
	bi_smr_free(old);
}

void
bi_msg_handler(void *msg, size_t sz, int nid, int cid)
{
	struct atomic_obj_msg *req;
	int r;

	assert(msg);
	assert(sz == sizeof(struct atomic_obj_msg));

	req = (struct atomic_obj_msg *)msg;
	atomic_obj_write_svr(req->val);
	r   = rpc_send_server(nd, cd, &msg_reply, sz);
	assert(r == 0);
}

void *
writer_thd_fn(void *arg)
{
	struct thread_data *mythd;

	mythd = (struct thread_data *)arg;
	thd_set_affinity(pthread_self(), mythd->nd, mythd->cd);
	bi_local_init_server(mythd->cd, mythd->ncore);
	bi_server_run(bi_msg_handler);
}

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
atomic_obj_write(int id)
{
	struct atomic_obj_msg msg;
	int nd, r;
	size_t s;

	msg.val = id;
	nd      = id % get_active_node_num();
	r       = rpc_send(nd, &msg, sizeof(struct atomic_obj_msg));
	assert(r == 0);
	s       = rpc_recv(nd, &msg, 1);
	assert(s == sizeof(struct atomic_obj_msg));
	assert(msg.val == -1);
}

void
atomic_obj_read(int id)
{
	struct Test_obj *to;
	void *old;

	bi_enter();
	to  = get_test_obj(id);
	old = bi_dereference_pointer_lazy(&(to->data));
	assert(old);
	memcpy(temp_obj, old, to->sz);
	bi_exit();
}

void
spawn_writer(pthread_t thd, int nd, int cd, int ncore)
{
	int ret;

	ret = pthread_create(thd, 0, writer_thd_fn, &tds[cd]);
	if (ret) {
		printf("create writer nd %d cd %d failed\n", nd, cd);
		exit(-1);
	}
}

#endif
