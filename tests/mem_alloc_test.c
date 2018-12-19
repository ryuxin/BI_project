#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "args.h"

#define TEST_FILE_NAME "/lfs/cache_test"
#define TEST_FILE_SIZE (1024*1024*1024)
#define TEST_FILE_ADDR (NULL)
#define ITER 1024

struct Free_mem_item *ptrs[MEM_MGR_OBJ_NUM];

int
main(int argc, char *argv[])
{
	int i, j;
	void *addr, *mem;
	struct Mem_layout *layout;

	setup_core_id(0);
	test_parse_args(argc, argv);
	mem = bi_global_init_master(id_node, num_node, num_core,
				    TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR, 
				    "memory allocator tests");
	layout = (struct Mem_layout *)mem;
	printf("test: %s\n", layout->magic);

	assert(id_node == NODE_ID());
	addr = get_mem_start_addr(id_node);
	mem_mgr_init();

	for (i = ITER-1 ; i >= 0 ; i--) {
		for (j = 0; j<MEM_MGR_OBJ_NUM; j++) {
			ptrs[j] = mem_mgr_alloc(MEM_MGR_OBJ_SZ);
			assert(ptrs[j]);
			assert(ptrs[j]->addr);
			assert(ptrs[j]->addr == addr + j * MEM_MGR_OBJ_SZ);
			assert(ps_list_singleton_d(ptrs[j]));
		}
		for (j = 0; j<MEM_MGR_OBJ_NUM; j++) {
			mem_mgr_free(ptrs[j]);
			ptrs[j] = NULL;
		}
	}
	printf("Mem allocator unit tests:  SUCCESS!\n");

	return 0;
}
