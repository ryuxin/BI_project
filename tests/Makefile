include ../Makefile.inc

.PHONY: clean rpc_tests

all: clean rpc_tests

rpc_tests:
	@$(CC) rpc_test_client.c $(CFLAGS) -o rpc_test_client.test $(LDFLAGS)
	@$(CC) rpc_test_server.c $(CFLAGS) -o rpc_test_server.test $(LDFLAGS)

clean:
	rm -rf *.o *.d *.test
