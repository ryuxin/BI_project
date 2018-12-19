#ifndef BI_HEADER_H
#define BI_HEADER_H

#include "bi_pointer.h"
#include "rpc.h"
#include "mem_mgr.h"
#include "bi_slab.h"
#include "bi_smr.h"

/* a single master node init the global memory */
void *bi_global_init_master(int node_id, int node_num, int core_num, const char *test_file, long file_size, void *map_addr, char *test_string);
/* all other nodes map in the global memory */
void *bi_global_init_slave(int node_num, int node_id, const char *test_file, long file_size, void *map_addr);
/* core local reader init */
void bi_local_init_reader(int core_id);
/*TODO: core local init writer: update, parsec, data memory */

#endif /* BI_HEADER_H */
