#ifdef ATOMIC_OBJ_BI

#include <stdio.h>
#include <stdlib.h>
#include "atomic_obj.h"

struct atomic_obj_msg {
	int val; /* >0 is request object id, -1 is reply */
};

struct atomic_obj_msg msg_reply = { .val = -1 };
static int start_id[NUM_NODES], end_id[NUM_NODES];
extern int num_node, obj_num;

static inline int
id_to_node(int id)
{
	int i;
	for(i=0; i<num_node; i++) {
		if (id >= start_id[i] && id < end_id[i]) break;;
	}
	return i;
}

static inline void
atomic_obj_write_svr(int id)
{
	void *new_data, *old;
	struct Test_obj *to;

	assert(id >= start_id[NODE_ID()]);
	assert(id <  end_id[NODE_ID()]);
	to       = get_test_obj(id);
	old      = bi_dereference_pointer_lazy(to->data);
	new_data = bi_slab_alloc(slab_allocator);
	assert(to->data);
	bi_publish_area(new_data, temp_obj, to->sz);
	bi_publish_pointer(to->data, new_data);
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
	r   = rpc_send_server(nid, cid, &msg_reply, sz);
	assert(r == 0);
}

void
atomic_obj_flush(void)
{
	int nd = NODE_ID();
	void *sa, *ea;

	sa = get_test_obj(start_id[nd]);
	ea = get_test_obj(end_id[nd]);
	clwb_range_opt(sa, ea - sa);
}

static void *
writer_thd_fn(void *arg)
{
	struct thread_data *mythd;

	mythd = (struct thread_data *)arg;
	thd_set_affinity(pthread_self(), mythd->nd, mythd->cd);
	bi_local_init_server(mythd->cd, mythd->ncore);
	bi_server_run(bi_msg_handler, atomic_obj_flush);
	return NULL;
}

void
atomic_obj_init(int num, size_t sz)
{
	struct Test_obj *to;
	int nn, nc, q, i;

	nn = num_node;
	nc = obj_num/nn;
	q  = obj_num % nn;
	for(i=0; i<q; i++) {
		start_id[i] = i*(nc + 1);
		end_id[i]   = start_id[i] + nc+1;
	}
	for(; i<nn; i++) {
		start_id[i] = i*nc + q;
		end_id[i]   = start_id[i] + nc;
	}

	for(i=1; i<nn; i++) assert(end_id[i-1] == start_id[i]);
	assert(end_id[nn-1] == obj_num);
	assert(num <= MAX_TEST_OBJ_NUM);

	for(i=0; i<num; i++) {
		if (id_to_node(i) == NODE_ID()) {
			to       = get_test_obj(i);
			to->sz   = sz;
			to->data = bi_slab_alloc(slab_allocator);
		}
	}
}

void
atomic_obj_write(int id)
{
	struct atomic_obj_msg msg;
	int nd, r;
	size_t s;

	msg.val = id;
	nd      = id_to_node(id);
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
	old = bi_dereference_pointer_lazy(to->data);
	assert(old);
	bi_dereference_area_lazy(temp_obj, old, to->sz);
	bi_exit();
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
