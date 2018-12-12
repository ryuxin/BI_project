include ../Makefile.inc

.PHONY: clean rpc_tests cache_micro

all: clean rpc_tests cache_micro

rpc_tests:
	@$(CC) rpc_test_client.c $(CFLAGS) -o rpc_test_client.test $(LDFLAGS)
	@$(CC) rpc_test_server.c $(CFLAGS) -o rpc_test_server.test $(LDFLAGS)

cache_micro:
	@$(CC) cache_micro.c $(CFLAGS) -o cache_micro.test $(LDFLAGS)

clean:
	rm -rf *.o *.d *.test
