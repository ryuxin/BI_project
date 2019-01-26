#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define _GNU_SOURCE
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "hw_util.h"
#include "args.h"

#define TEST_FILE_ADDR ((void *)0x7ffeb760e000)
#define MEM_SZ     (1<<26) 	/* 64MB */
#define ITER       1024

typedef enum {
	SIZES,
	READ,
	WRITE,
	SUCC,
	EXIT,
} access_t;

struct rpc_msg {
	access_t type;
	size_t sz;
};

struct rpc_msg msg_reply, msg_req;
char tmem[MEM_SZ];
void *mem;
static volatile uint64_t use_result_dummy = 0;

inline uint64_t
rdtsc(void)
{
	unsigned long a, d;

	asm volatile ("rdtscp" : "=a" (a), "=d" (d) : : "ebx", "ecx");

	return ((uint64_t)d << 32) | (uint64_t)a;
}

void
use_int(int result) { use_result_dummy += result; }

static inline void
rd(size_t sz)
{	
	register int *lastone = mem + sz;
	register int sum = 0;
	register int *p = mem;

	while (p <= lastone) {
		sum += 
#define	DOIT(i)	p[i]+
		DOIT(0) DOIT(4) DOIT(8) DOIT(12) DOIT(16) DOIT(20) DOIT(24)
		DOIT(28) DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52)
		DOIT(56) DOIT(60) DOIT(64) DOIT(68) DOIT(72) DOIT(76)
		DOIT(80) DOIT(84) DOIT(88) DOIT(92) DOIT(96) DOIT(100)
		DOIT(104) DOIT(108) DOIT(112) DOIT(116) DOIT(120) 
		p[124];
		p +=  128;
	}
	use_int(sum);
}
#undef	DOIT

static inline void
wr(size_t sz)
{	
	register int *lastone = mem + sz;
	register int *p = mem;

	while (p <= lastone) {
#define	DOIT(i)	p[i] = 1;
		DOIT(0) DOIT(4) DOIT(8) DOIT(12) DOIT(16) DOIT(20) DOIT(24)
		DOIT(28) DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52)
		DOIT(56) DOIT(60) DOIT(64) DOIT(68) DOIT(72) DOIT(76)
		DOIT(80) DOIT(84) DOIT(88) DOIT(92) DOIT(96) DOIT(100)
		DOIT(104) DOIT(108) DOIT(112) DOIT(116) DOIT(120) DOIT(124);
		p +=  128;
	}
}

static inline void
mem_op(access_t how, size_t sz)
{
	assert(sz >= CACHE_LINE);

	switch(how) {
	case READ:   rd(sz);            break;
	case WRITE:  wr(sz);            break;
	default:     return;
	}
}

void
rpc_op(access_t how, size_t sz)
{
	int r;
	size_t s;

	assert(sz >= CACHE_LINE);
	if (how == SIZES) return ;
	msg_req.type = how;
	msg_req.sz   = sz;
	r = rpc_send(0, &msg_req, sizeof(struct rpc_msg));
	assert(r == 0);
	s = rpc_recv(0, &msg_reply, 1);
	assert(s == sizeof(struct rpc_msg));
	assert(msg_reply.type == SUCC);
}

static void
rpc_svr_op(void)
{
	int r;
        size_t s;
	int nd, cd;

	while (1) {
		s = rpc_recv_server(&msg_req, &nd, &cd);
		if (!s) continue;
		assert(nd == 1);
		assert(cd == 0);
		assert(s == sizeof(struct rpc_msg));
		mem_op(msg_req.type, msg_req.sz);
		r = rpc_send_server(nd, cd, &msg_reply, sizeof(struct rpc_msg));
		assert(r == 0);
		if (msg_req.type == EXIT) break;
	}
}

void
exec(char *name, access_t *how_ops, size_t nops)
{
	size_t i;
	static unsigned long sizes[] = {1<<6, 1<<8, 1<<10, 1<<12, 1<<14, 1<<16, 1<<18, 1<<20, 1<<22, 1<<24, 1<<26};
	int iter;
	uint64_t start, end, tot = 0, overhead;
	void *src, *dst;
	(void)name;

	for (iter = 0 ; iter < ITER ; iter++) {
		start = rdtsc();
		end   = rdtsc();
		assert(end-start > 0);
		tot  += end-start;
	}
	overhead = tot;
	tot      = 0;

	assert(how_ops && nops > 0);
#ifndef NO_PRINT_TITLES
	printf("%20s\t", name);
#endif
	for (i = 0 ; i < sizeof(sizes)/sizeof(unsigned long) ; i++) {
		for (tot = 0, iter = 0 ; iter < ITER ; iter++) {
			rpc_op(how_ops[0], sizes[i]);
			if (how_ops[nops-1] == WRITE) src = mem, dst = tmem;
			else if (how_ops[nops-1] == READ) src = tmem, dst = mem;
			else break;
			bi_mb();
			bi_ccb();
			start = rdtsc();
//			memcpy(dst, src, sizes[i]);
			mem_op(how_ops[nops-1], sizes[i]);
			end   = rdtsc();
			tot  += end-start;
		}
		if (how_ops[nops-1] != SIZES) printf("%lu ", (tot-overhead)/ITER);
		else                          printf("%lu ", sizes[i]);
	}
	printf("\n");
}

int
main(int argc, char *argv[])
{
	struct Mem_layout *g_layout;
	void *tm;

	test_parse_args(argc, argv);
	if (!id_node) {
		tm = bi_global_init_master(id_node, num_node, num_core,
					    TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR, 
					    "read write multi-node benchmark");
		bi_set_barrier(1);
	} else {
		tm = bi_global_init_slave(id_node, num_node, num_core,
					   TEST_FILE_NAME, TEST_FILE_SIZE, TEST_FILE_ADDR);
		bi_wait_barrier(1);
		mem_mgr_init();
	}
	g_layout = (struct Mem_layout *)tm;
	mem      = g_layout->data_area;
	msg_reply.type = SUCC;
	setup_core_id(0);
	if (!id_node) {
		rpc_svr_op();
	} else {
		exec("Sizes", (access_t[1]){SIZES}, 1);

		exec("Read+Read", (access_t[2]){READ, READ}, 2);
		exec("Modify+Read", (access_t[2]){WRITE, READ}, 2);
		/* exec("Flush + Read", (access_t[2]){FLUSHOPT, READ}, 2); */

		exec("Read+Modify", (access_t[2]){READ, WRITE}, 2);
		exec("Modify+Modify", (access_t[2]){WRITE, WRITE}, 2);
		rpc_op(EXIT, 100);
	}

	return 0;
}
