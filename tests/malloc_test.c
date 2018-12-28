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
#include <pthread.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "args.h"

#define TEST_FILE_ADDR (NULL)
#define ITER 1024
#define TEST_THD_NUM 8

static void
tests(size_t sz, void **ptrs)
{
	int i, j;
	for (i = ITER-1 ; i >= 0 ; i--) {
		for (j = i ; j < ITER ; j++) {
			ptrs[j] = bi_malloc(sz);
			assert(ptrs[j]);
		}
		for (j = i ; j < ITER ; j++) {
			bi_free(ptrs[j]);
			ptrs[j] = NULL;
		}
	}
}

static void *
thread_test_fn(void *arg)
{
	void **ptrs;
	long cd = (long)arg;

	thd_set_affinity(pthread_self(), 0, (int)cd);
	ptrs = (void **)malloc(ITER * sizeof(void *));
	tests(10, ptrs);
	tests(100, ptrs);
	tests(500, ptrs);
	free(ptrs);
	return NULL;
}

int
main(int argc, char *argv[])
{
	int i, ret;
	void *mem;
	struct Mem_layout *layout;
	pthread_t pthds[TEST_THD_NUM];

	setup_core_id(0);
	test_parse_args(argc, argv);
	mem = bi_global_init_master(id_node, num_node, num_core,
				    TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR, 
				    "bi mallocc tests");
	layout = (struct Mem_layout *)mem;
	printf("test: %s\n", layout->magic);

	assert(id_node == NODE_ID());

	for(i=0; i<TEST_THD_NUM; i++) {
		ret = pthread_create(&pthds[i], 0, thread_test_fn, (void *)(long)i);
		if (ret) {
			perror("pthread create of child\n");
			exit(-1);
		}
	}
	sleep(3);
	for(i=0; i<TEST_THD_NUM; i++) {
		pthread_join(pthds[i], NULL);
	}
	printf("BI malloc unit tests:  SUCCESS!\n");

	return 0;
}
