#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include "atomic_obj.h"

#define TEST_FILE_NAME "/lfs/cache_test"
#define TEST_FILE_SIZE (5*1024*1024*1024UL)
#define TEST_FILE_ADDR ((void *)0x7fffb75f4000)
#define N_OPS 10000000
#define MULTIPLIER 5101

struct ps_slab_info *slab_allocator;
char *temp_obj;
struct thread_data tds[NUM_CORE_PER_NODE];
static int num_core, id_node;
int num_node, obj_num;
static size_t obj_sz;
static char ops[N_OPS];
static int tot_thput = 0;

static void
usage(void)
{
	printf("atomic object bench: \n"
	" Options:\n"
	"  -i <id>         id of this partition (from 0)\n"
	"  -n N            number of partitions\n"
	"  -c N            number of core in each partition\n"
	"  -m N            number of test objects\n"
	"  -s N            size of test objects\n"
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
	while ((opt=getopt(argc, argv, "i:n:c:m:s:h:")) != EOF) {
		switch (opt) {
		case 'i':
			id_node  = atoi(optarg);
			break;
		case 'n':
			num_node = atoi(optarg);
			break;
		case 'c':
			num_core = atoi(optarg);
			break;
		case 'm':
			obj_num  = atoi(optarg);
			break;
		case 's':
			obj_sz   = (size_t)atoi(optarg);
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
	if (obj_num > MAX_TEST_OBJ_NUM) {
		printf("error: number of obj %d is out of range %d\n", obj_num, MAX_TEST_OBJ_NUM);
		exit(-1);
	}
}

void
print_re(struct thread_data *mythd)
{
	uint64_t avg_r = 0, avg_w = 0, avg = 0, thput=0;

	if (mythd->nread) avg_r   = mythd->rtsc/mythd->nread;
	if (mythd->nupdate) avg_w = mythd->wtsc/mythd->nupdate;
	if (mythd->nread + mythd->nupdate) {
		avg = mythd->tot_tsc/(mythd->nread + mythd->nupdate);
	}
	if (avg) thput = CPU_HZ/avg;
	tot_thput += thput;
	printf("node %d core %d tot %d ops (r %d, u %d) done, %lu (r %lu, w %lu) cycles per op, thput %lu\n",
			mythd->nd, mythd->cd,
			mythd->nread + mythd->nupdate, mythd->nread, mythd->nupdate, 
			avg, avg_r, avg_w, thput);
}

void
bench(struct thread_data *mythd)
{
	int n_read = 0, n_update = 0;
	int i, jump, id, nobj;
	uint64_t s, e, s1, e1, cost_r = 0, cost_w = 0, cost;

	assert(mythd->ncore == get_active_core_num());
	assert(mythd->nd == NODE_ID());
	assert(mythd->cd == CORE_ID());
	jump = mythd->ncore;
	nobj = mythd->nobj;
	id   = rand() % nobj;

	s = bi_local_rdtsc();
	for(i=mythd->cd; i<N_OPS; i+=jump) {
		s1 = bi_local_rdtsc();
		if (ops[i]) {
			atomic_obj_write(id);
			e1      = bi_local_rdtsc();
			cost    = e1-s1;
			cost_w += cost;
			n_update++;
		} else {
			atomic_obj_read(id);
			e1      = bi_local_rdtsc();
			cost    = e1-s1;
			cost_r += cost;
			n_read++;
		}
		id = (id+MULTIPLIER) % nobj;
		assert(e1 > s1);
	}
	e = bi_local_rdtsc();

	assert(n_read + n_update <= N_OPS);
	mythd->nread   = n_read;
	mythd->nupdate = n_update;
	mythd->rtsc    = cost_r;
	mythd->wtsc    = cost_w;
	mythd->tot_tsc = e-s;
}

void *
reader_thd_fn(void *arg)
{
	struct thread_data *mythd;

	mythd = (struct thread_data *)arg;
	thd_set_affinity(pthread_self(), mythd->nd, mythd->cd);
	bi_local_init_reader(mythd->cd);
	bench(mythd);
	return NULL;
}

void
spawn_reader(pthread_t *thd, int nd, int cd)
{
	int ret;

	ret = pthread_create(thd, 0, reader_thd_fn, &tds[cd]);
	if (ret) {
		printf("create reader nd %d cd %d failed\n", nd, cd);
		exit(-1);
	}
}

int
main(int argc, char *argv[])
{
	void *mem;
	int i, ret;
	struct Mem_layout *layout;
	pthread_t pthds[NUM_CORE_PER_NODE];

	test_parse_args(argc, argv);
	if (!id_node) {
		mem = bi_global_init_master(id_node, num_node, num_core,
					    TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR, 
					    "atomic object benchmark");
		bi_set_barrier(1);
	} else {
		mem = bi_global_init_slave(id_node, num_node, num_core,
					   TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR);
		bi_wait_barrier(1);
		mem_mgr_init();
	}
	layout         = (struct Mem_layout *)mem;
	slab_allocator = bi_slab_create(obj_sz);
	temp_obj       = malloc(obj_sz);
	printf("magic: %s\n", layout->magic);
	memset(temp_obj, '$', obj_sz);
	srand(time(NULL));
	load_trace(N_OPS, 50, ops);
	if (!id_node) {
		atomic_obj_init(obj_num, obj_sz);
		usleep(100000);
		bi_set_barrier(2);
	} else {
		bi_wait_barrier(2);
	}
	__init_thd_data(id_node, 0, num_core, obj_num);
	spawn_writer(&pthds[0], id_node, 0);
	for(i=1; i<num_core; i++) {
		__init_thd_data(id_node, i, num_core, obj_num);
		spawn_reader(&pthds[i], id_node, i);
	}
	usleep(50000);
	join_wirter(pthds[0]);
	for (i = 1; i < num_core; i++) {
		pthread_join(pthds[i], (void *)&ret);
	}
	for (i = 0; i < num_core; i++) {
		print_re(&tds[i]);
	}
	printf("node %d ncore %d obj num %d sz %lu thput %d\n", num_node, num_core, obj_num, obj_sz, tot_thput);
	sleep(10);
}
