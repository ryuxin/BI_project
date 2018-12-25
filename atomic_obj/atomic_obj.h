#ifndef ATOMIC_OBJ_H
#define ATOMIC_OBJ_H

#include "bi.h"

struct thread_data {
	int nd, cd, ncore, nobj;
	int nread, nupdate;
	uint64_t rtsc, wtsc, tot_tsc;
} __attribute__((aligned(CACHE_LINE), packed));

extern struct ps_slab_info *slab_allocator;
extern struct thread_data tds[NUM_CORE_PER_NODE];
extern char *temp_obj;

static inline void
__init_thd_data(int nd, int cd, int ncore, int nobj)
{
	memset(&tds[cd], 0, sizeof(struct thread_data));
	tds[cd].nd    = nd;
	tds[cd].cd    = cd;
	tds[cd].ncore = ncore;
	tds[cd].nobj  = nobj;
}

void atomic_obj_init(int num, size_t sz);
void atomic_obj_read(int id);
void atomic_obj_write(int id);
void spawn_writer(pthread_t *thd, int nd, int cd);
void spawn_reader(pthread_t *thd, int nd, int cd);
void join_wirter(pthread_t thd);

#endif /* ATOMIC_OBJ_H */
