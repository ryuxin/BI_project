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
#include <time.h> 
#include <pthread.h>
#define _GNU_SOURCE
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "bi.h"
#include "args.h"

//#define TEST_BI
//#define TEST_RCU
//#define TEST_LOCK
#ifdef TEST_BI
#define TEST_FILE_ADDR ((void *)0x7f5e775f4000)
#endif
#ifdef TEST_RCU
//#include <urcu.h>
#include <urcu/urcu-mb.h>
#include "bi_rcu.h"
#define TEST_FILE_ADDR ((void *)0x7f5e771e7000)
extern void rcu_bi_init(void);
#endif
#ifdef TEST_LOCk
#define TEST_FILE_ADDR ((void *)0x7f5e771e7000)
#endif
#define N_OPS 10000000

struct thread_data {
	int nd, cd, ncore;
	int nread, nupdate;
	uint64_t rtsc, wtsc, tot_tsc;
} __attribute__((aligned(CACHE_LINE), packed));

static int updater = 50; // hack with -t option
static char ops[N_OPS];
static unsigned long rp99_log[N_OPS];
static unsigned long wp99_log[N_OPS];
static struct thread_data tds[NUM_CORE_PER_NODE];
static struct ck_spinlock_mcs **global_mcs_lock;
static int tot_thput = 0;
static volatile int run = 0;

static inline void
__init_thd_data(int nd, int cd, int ncore)
{
	memset(&tds[cd], 0, sizeof(struct thread_data));
	tds[cd].nd    = nd;
	tds[cd].cd    = cd;
	tds[cd].ncore = ncore;
}

static int cmpfunc(const void * a, const void * b)
{
	return ( *(int*)a - *(int*)b );
}

static void
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
	if (mythd->cd == 0) {
		unsigned long r_99 = 0, w_99 = 0;
		if (mythd->nread) {
			qsort(rp99_log, mythd->nread, sizeof(unsigned long), cmpfunc);
			r_99 = rp99_log[mythd->nread - mythd->nread / 100];
		}
		if (mythd->nupdate) {
			qsort(wp99_log, mythd->nupdate, sizeof(unsigned long), cmpfunc);
			w_99 = wp99_log[mythd->nupdate - mythd->nupdate / 100];			
		}
		printf("99p: read %lu write %lu\n", r_99, w_99);
	}
}

static void
test_init()
{
#ifdef TEST_RCU
	if (!NODE_ID()) bi_rcu_init_global((struct RCU_block *)get_rcu_area());
	bi_rcu_init_local();
	rcu_bi_init();
#endif
	global_mcs_lock = get_mcs_lock(0);
}

static void
reader_fn(ck_spinlock_mcs_t cntxt)
{
#ifdef TEST_BI
	(void)cntxt;
	bi_enter();
	bi_exit();
#endif
#ifdef TEST_RCU
	(void)cntxt;
	urcu_mb_read_lock();
	urcu_mb_read_unlock();
#endif
#ifdef TEST_LOCk
	ck_spinlock_mcs_lock(global_mcs_lock, cntxt);
	ck_spinlock_mcs_unlock(global_mcs_lock, cntxt);
#endif
}

static void
writer_fn(ck_spinlock_mcs_t cntxt)
{
#ifdef TEST_BI
	(void)cntxt;
	bi_time_flush();
	bi_quiesce(bi_local_rdtsc());
#endif
#ifdef TEST_RCU
	(void)cntxt;
	urcu_mb_synchronize_rcu();
#endif
#ifdef TEST_LOCk
	ck_spinlock_mcs_lock(global_mcs_lock, cntxt);
	ck_spinlock_mcs_unlock(global_mcs_lock, cntxt);
#endif
}

void
bench(struct thread_data *mythd)
{
	int n_read = 0, n_update = 0;
	int i, jump;
	uint64_t s, e, s1, e1, cost_r = 0, cost_w = 0, cost;
	ck_spinlock_mcs_t cntxt;

	assert(mythd->ncore == get_active_core_num());
	assert(mythd->nd == NODE_ID());
	assert(mythd->cd == CORE_ID());
	jump = mythd->ncore;
	cntxt = get_mcs_lock_cntxt();

	s = bi_local_rdtsc();
	for(i=mythd->cd; i<N_OPS; i+=jump) {
//		if (i%10000 == 0) printf("dbg i %d op %d\n", i, ops[i]);
//		if (i == 10000 ) break;
		s1 = bi_local_rdtsc();
		if (ops[i]) {
			writer_fn(cntxt);
			e1      = bi_local_rdtsc();
			cost    = e1-s1;
			cost_w += cost;
			if (mythd->cd == 0) wp99_log[n_update] = cost;
			n_update++;
		} else {
			reader_fn(cntxt);
			e1      = bi_local_rdtsc();
			cost    = e1-s1;
			cost_r += cost;
			if (mythd->cd == 0) rp99_log[n_read] = cost;
			n_read++;
		}
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
test_fn(void *arg)
{
	struct thread_data *mythd;
	mythd = (struct thread_data *)arg;
	thd_set_affinity(pthread_self(), mythd->nd, mythd->cd);
	bi_local_init_reader(mythd->cd);
	while (run == 0) ;
	bench(mythd);
	return NULL;
}

int main(int argc, char *argv[])
{
	void *mem;
	struct Mem_layout *layout;
	pthread_t pthds[NUM_CORE_PER_NODE];
	int i, ret;

	srand(time(0));
	test_parse_args(argc, argv);
	thd_set_affinity(pthread_self(), id_node, 0);
	if (!id_node) {
		mem = bi_global_init_master(id_node, num_node, num_core,
					    TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR, 
	 				    "smr comparsion benchmark");
	} else {
		mem = bi_global_init_slave(id_node, num_node, num_core,
					   TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR);
		mem_mgr_init();
	}
	layout = (struct Mem_layout *)mem;
	printf("test: %s\n", layout->magic);
	if (test_case) updater = test_case;
	updater = test_case;
	load_trace(N_OPS, updater, ops);
	test_init();
	for(i=0; i<num_core; i++) {
		__init_thd_data(id_node, i, num_core);
		ret = pthread_create(&pthds[i], 0, test_fn, &tds[i]);
		if (ret) {
			printf("create reader nd %d cd %d failed\n", id_node, i);
			exit(-1);
		}
	}
	usleep(50);
	run = 1;
	usleep(50000);
	for (i = 0; i < num_core; i++) {
		pthread_join(pthds[i], (void *)&ret);
	}
	for (i = 0; i < num_core; i++) {
		print_re(&tds[i]);
	}
	printf("node %d ncore %d update %d thput %d\n", num_node, num_core, updater, tot_thput);
	sleep(10);
	return 0;
}
