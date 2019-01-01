#ifndef ARGS_H
#define ARGS_H
#include "bi.h"

#define TEST_FILE_NAME "/lfs/cache_test"
#define TEST_FILE_SIZE (5*1024*1024*1024UL)

static int num_node, num_core, id_node;
static int test_case;

static void
usage(void)
{
	printf("rpc test: client\n"
	" Options:\n"
	"  -i <id>         id of this partition (from 0)\n"
	"  -n N            number of partitions\n"
	"  -c N            number of core in each partition\n"
	"  -t N            test case 1 - basic test, 2 - benchmark\n"
	"  -h              help\n"
	"\n");
}

static void
test_parse_args(int argc, char **argv)
{
	char opt;

	if (argc < 4) {
		usage();
		exit(-1);
	}
	while ((opt=getopt(argc, argv, "i:n:c:t:h:")) != EOF) {
		switch (opt) {
		case 'i':
			id_node = atoi(optarg);
			break;
		case 'n':
			num_node = atoi(optarg);
			break;
		case 'c':
			num_core = atoi(optarg);
			break;
		case 't':
			test_case = atoi(optarg);
			break;
		case 'h':
			usage();
			exit(0);
		}
	}
	if (num_node > NUM_NODES) {
		printf("error: number of partition %d is larger than max %d\n", num_node, NUM_NODES);
		exit(-1);
	}
	if (num_core > NUM_CORE_PER_NODE) {
		printf("error: number of core %d is larger than max %d\n", num_core, NUM_CORE_PER_NODE);
		exit(-1);
	}
	if (id_node >= num_node) {
		printf("error: partition id %d is out of range %d\n", id_node, num_node);
		exit(-1);
	}
}

#endif /* ARGS_H */
