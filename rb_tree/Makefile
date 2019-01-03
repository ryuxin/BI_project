include Makefile.inc

CFILES  = $(wildcard *.c)
COBJS   = $(patsubst %.c,%.o,$(CFILES))

.PHONY: clean all bi urcu

all: bi urcu

bi: $(CFILES)
	@rm -rf *.o
	@$(CC) $^ $(CFLAGS) -DRBTREE_BI -DRBTREE_TEST -o rbtree_bi_correct.test $(LDFLAGS)
	@rm -rf *.o
	@$(CC) $^ $(CFLAGS) -DRBTREE_BI -o rbtree_bi_bench.test $(LDFLAGS)

urcu: $(CFILES)
	@rm -rf *.o
	@$(CC) $^ $(CFLAGS) -DRBTREE_URCU -o rbtree_rcu_bench.test $(LDFLAGS) -lurcu -lbircu

clean:
	@rm -f *.o *.d *.test *~
