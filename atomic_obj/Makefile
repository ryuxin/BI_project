include Makefile.inc

CFILES  = $(wildcard *.c)
COBJS   = $(patsubst %.c,%.o,$(CFILES))

.PHONY: clean all bi lock

all: bi lock

bi: $(CFILES)
	@rm -rf *.o
	@$(CC) $^ $(CFLAGS) -DATOMIC_OBJ_BI -o aobj_bi.test $(LDFLAGS)

lock: $(CFILES)
	@rm -rf *.o
	@$(CC) $^ $(CFLAGS) -DATOMIC_OBJ_LOCK -o aobj_lock.test $(LDFLAGS)

clean:
	@rm -f *.o *.d *.test *~
