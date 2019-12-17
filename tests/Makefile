include ../Makefile.inc

.PHONY: clean rpc_tests cache_micro flush_period mem_alloc slab smr urcu rw_bench cache_lat smr_bench

all: clean rpc_tests cache_micro flush_period mem_alloc slab smr urcu rw_bench cache_lat smr_bench

rpc_tests:
	@$(CC) rpc_test_client.c $(CFLAGS) -o rpc_test_client.test $(LDFLAGS)
	@$(CC) rpc_test_server.c $(CFLAGS) -o rpc_test_server.test $(LDFLAGS)

cache_micro:
	@$(CC) cache_micro.c $(CFLAGS) -o cache_micro.test $(LDFLAGS)

flush_period:
	@$(CC) flush_period.c $(CFLAGS) -o flush_period.test $(LDFLAGS) -lrt
	@$(CC) flush_period_multi_thd.c $(CFLAGS) -o flush_period_2thd.test $(LDFLAGS)

mem_alloc:
	@$(CC) mem_alloc_test.c $(CFLAGS) -o mem_alloc.test $(LDFLAGS)
	@$(CC) malloc_test.c $(CFLAGS) -o malloc.test $(LDFLAGS)

slab:
	@$(CC) slab_test.c $(CFLAGS) -o slab.test $(LDFLAGS)

smr:
	@$(CC) smr_test.c $(CFLAGS) -o smr.test $(LDFLAGS)

urcu:
	@$(CC) urcu.c $(CFLAGS) -o urcu.test $(LDFLAGS) -lurcu -lbircu

rw_bench:
	@$(CC) read_write_bench.c $(CFLAGS) -o rwbench.test $(LDFLAGS)

cache_lat:
	@$(CC) cache_lat.c  $(CFLAGS) -o cache_lat.bench $(LDFLAGS)

smr_bench:
	@$(CC) smr_bench.c  $(CFLAGS) -DTEST_BI -o smr_bi.bench $(LDFLAGS)
	@$(CC) smr_bench.c  $(CFLAGS) -DTEST_RCU -o smr_rcu.bench $(LDFLAGS) -lurcu-mb -lbircu
	@$(CC) smr_bench.c  $(CFLAGS) -DTEST_LOCk -o smr_lock.bench $(LDFLAGS)

clean:
	rm -rf *.o *.d *.test *.bench
