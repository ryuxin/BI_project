#ifndef RPC_TEST_COMMON_H
#define RPC_TEST_COMMON_H
#include "bi.h"
#include "args.h"

#define TEST_FILE_NAME "/lfs/cache_test"
#define TEST_FILE_SIZE (1024*1024*1024)
#define TEST_FILE_ADDR (NULL)
#define TEST_MSG_SIZE (128)

struct Rpc_test_msg {
	char message[TEST_MSG_SIZE];
};

struct Rpc_test_msg test_snt_msg, test_rcv_msg;

static void
non_cc_test(struct Mem_layout *layout)
{
	int test;

	printf("non cc test magic %s\n", layout->magic);
	while (1) {
		scanf("%d", &test);
		if (test == 0) break;
		switch (test) {
		case 1:
			printf("magic %s\n", layout->magic);
			break;				
		case 2:
			strcpy(layout->magic, "modified");
			break;
		case 3:
			bi_flush_cache(layout->magic);
			break;
		case 4:
			bi_wb_cache(layout->magic);
			break;
		}
	}
}

#endif /* RPC_TEST_COMMON_H */
