include ../Makefile.inc

.PHONY: clean rpc_tests cache_micro flush_period mem_alloc slab smr

all: clean rpc_tests cache_micro flush_period mem_alloc slab smr

rpc_tests:
	@$(CC) rpc_test_client.c $(CFLAGS) -o rpc_test_client.test $(LDFLAGS)
	@$(CC) rpc_test_server.c $(CFLAGS) -o rpc_test_server.test $(LDFLAGS)

cache_micro:
	@$(CC) cache_micro.c $(CFLAGS) -o cache_micro.test $(LDFLAGS)

flush_period:
	@$(CC) flush_period.c $(CFLAGS) -o flush_period.test $(LDFLAGS) -lrt

mem_alloc:
	@$(CC) mem_alloc_test.c $(CFLAGS) -o mem_alloc.test $(LDFLAGS)
	@$(CC) malloc_test.c $(CFLAGS) -o malloc.test $(LDFLAGS)

slab:
	@$(CC) slab_test.c $(CFLAGS) -o slab.test $(LDFLAGS)

smr:
	@$(CC) smr_test.c $(CFLAGS) -o smr.test $(LDFLAGS)

clean:
	rm -rf *.o *.d *.test
