#ifndef BI_HEADER_H
#define BI_HEADER_H

#include "bi_pointer.h"
#include "rpc.h"
#include "mem_mgr.h"
#include "bi_slab.h"
#include "bi_smr.h"

typedef void (*bi_update_fn_t)(void *msg, size_t sz, int nid, int cid);
typedef void (*bi_flush_fn_t)(void);

extern volatile int running_cores;

static inline void
bi_reader_exit(void)
{
	bi_faa((unsigned long *)&running_cores, -1);
}

/* a single master node init the global memory */
void *bi_global_init_master(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr, char *test_string);
/* all other nodes map in the global memory */
void *bi_global_init_slave(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr);
/* core local reader init */
void bi_local_init_reader(int core_id);
/* core local BI server/writer init */
void bi_local_init_server(int core_id, int ncore);
/* BI server run loop, finish after all other reader exit */
void bi_server_run(bi_update_fn_t update_fn, bi_flush_fn_t flush_fn);
void bi_server_stop(void);

#endif /* BI_HEADER_H */
