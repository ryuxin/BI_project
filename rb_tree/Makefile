include Makefile.inc

CFILES  = $(wildcard *.c)
COBJS   = $(patsubst %.c,%.o,$(CFILES))

.PHONY: clean all bi

all: bi

bi: $(CFILES)
	@rm -rf *.o
	@$(CC) $^ $(CFLAGS) -DRBTREE_BI -DRBTREE_TEST -o rbtree_bi_correct.test $(LDFLAGS)
	@rm -rf *.o
	@$(CC) $^ $(CFLAGS) -DRBTREE_BI -o rbtree_bi_bench.test $(LDFLAGS)

lock: $(CFILES)
	@rm -rf *.o
	@$(CC) $^ $(CFLAGS) -DATOMIC_OBJ_LOCK -o aobj_lock.test $(LDFLAGS)

clean:
	@rm -f *.o *.d *.test *~
