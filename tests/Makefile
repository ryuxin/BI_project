include ../Makefile.inc

.PHONY: clean rpc_tests

all: clean rpc_tests

rpc_tests:
	@$(CC) rpc_test_client.c $(CFLAGS) -o rpc_test_client.test $(LDFLAGS)

clean:
	rm -rf *.o *.d *.test
